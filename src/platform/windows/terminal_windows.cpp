#include "terminal/terminal.hpp"

#include "utils/env.hpp"
#include "utils/string_utils.hpp"

#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <string>

// The only place in the project where <windows.h> appears.  Everything the rest
// of the application sees is the platform-neutral Terminal interface.

namespace mdview::terminal {

namespace {

constexpr DWORD kInputWaitMillis = 16;
constexpr DWORD kMaxRecordsPerDrain = 64;

unsigned modifiersFromControlState(DWORD state) {
    unsigned mods = ModNone;
    if (state & SHIFT_PRESSED) mods |= ModShift;
    if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) mods |= ModAlt;
    if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) mods |= ModCtrl;
    return mods;
}

Key keyFromVirtualKey(WORD virtualKey) {
    switch (virtualKey) {
        case VK_UP:
            return Key::Up;
        case VK_DOWN:
            return Key::Down;
        case VK_LEFT:
            return Key::Left;
        case VK_RIGHT:
            return Key::Right;
        case VK_RETURN:
            return Key::Enter;
        case VK_ESCAPE:
            return Key::Escape;
        case VK_TAB:
            return Key::Tab;
        case VK_BACK:
            return Key::Backspace;
        case VK_DELETE:
            return Key::Delete;
        case VK_INSERT:
            return Key::Insert;
        case VK_PRIOR:
            return Key::PageUp;
        case VK_NEXT:
            return Key::PageDown;
        case VK_HOME:
            return Key::Home;
        case VK_END:
            return Key::End;
        case VK_F1:
            return Key::F1;
        case VK_F2:
            return Key::F2;
        case VK_F3:
            return Key::F3;
        case VK_F4:
            return Key::F4;
        case VK_F5:
            return Key::F5;
        case VK_F6:
            return Key::F6;
        case VK_F7:
            return Key::F7;
        case VK_F8:
            return Key::F8;
        case VK_F9:
            return Key::F9;
        case VK_F10:
            return Key::F10;
        case VK_F11:
            return Key::F11;
        case VK_F12:
            return Key::F12;
        default:
            return Key::None;
    }
}

ColorMode detectColorMode() {
    // NO_COLOR is a widely respected opt-out: https://no-color.org
    if (env::get("NO_COLOR")) return ColorMode::None;

    if (const auto colorTerm = env::get("COLORTERM")) {
        if (str::equalsIgnoreCaseAscii(*colorTerm, "truecolor") ||
            str::equalsIgnoreCaseAscii(*colorTerm, "24bit")) {
            return ColorMode::TrueColor;
        }
    }
    // Windows Terminal, ConEmu and WezTerm all render 24-bit colour.
    if (env::get("WT_SESSION") || env::get("ConEmuANSI") || env::get("WEZTERM_PANE")) {
        return ColorMode::TrueColor;
    }
    // Legacy conhost understands SGR colour but has no reliable 24-bit path;
    // the 256 colour palette is always a safe superset of the basic one.
    return ColorMode::Ansi256;
}

class WindowsTerminal final : public Terminal {
public:
    WindowsTerminal() = default;

    ~WindowsTerminal() override { shutdown(); }

    void initialize() override {
        if (initialized_) return;
        initialized_ = true;

        outputHandle_ = NormaliseHandle(::GetStdHandle(STD_OUTPUT_HANDLE));
        inputHandle_ = NormaliseHandle(::GetStdHandle(STD_INPUT_HANDLE));

        // --- output ------------------------------------------------------
        if (outputHandle_) {
            DWORD mode = 0;
            if (::GetConsoleMode(outputHandle_, &mode)) {
                outputIsConsole_ = true;
                originalOutputMode_ = mode;
                const DWORD desired =
                    mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
                if (::SetConsoleMode(outputHandle_, desired)) {
                    virtualTerminal_ = true;
                }
            }
        }

        originalOutputCodePage_ = ::GetConsoleOutputCP();
        originalInputCodePage_ = ::GetConsoleCP();
        utf8_ = ::SetConsoleOutputCP(CP_UTF8) != FALSE;
        ::SetConsoleCP(CP_UTF8);

        // Raw bytes out: our escape sequences must not be CRLF-translated.
        _setmode(_fileno(stdout), _O_BINARY);

        // --- input -------------------------------------------------------
        if (inputHandle_) {
            DWORD mode = 0;
            if (::GetConsoleMode(inputHandle_, &mode)) {
                inputIsConsole_ = true;
                originalInputMode_ = mode;
                DWORD desired = mode;
                // Turn off line buffering, echo and Ctrl+C processing so that
                // every keystroke (including Ctrl+C) arrives as a record.
                desired &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
                             ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE);
                desired |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
                inputReady_ = ::SetConsoleMode(inputHandle_, desired) != FALSE;
            }
        }
        _setmode(_fileno(stdin), _O_BINARY);

        // --- screen ------------------------------------------------------
        if (virtualTerminal_) {
            write("\x1b[?1049h");  // alternate screen buffer
            write("\x1b[?25l");    // hide the cursor
            write("\x1b[2J\x1b[H");
        } else {
            clear();
        }
    }

    void shutdown() override {
        if (!initialized_) return;
        initialized_ = false;

        if (virtualTerminal_) {
            write("\x1b[0m\x1b[?25h\x1b[?1049l");
        } else {
            showCursor(true);
        }

        if (inputReady_ && inputHandle_) ::SetConsoleMode(inputHandle_, originalInputMode_);
        if (outputIsConsole_ && outputHandle_) ::SetConsoleMode(outputHandle_, originalOutputMode_);
        if (originalOutputCodePage_ != 0) ::SetConsoleOutputCP(originalOutputCodePage_);
        if (originalInputCodePage_ != 0) ::SetConsoleCP(originalInputCodePage_);

        ::FlushFileBuffers(outputHandle_);
    }

    bool isInteractive() const override { return inputIsConsole_ && outputIsConsole_; }

    TerminalSize size() const override {
        if (outputHandle_) {
            CONSOLE_SCREEN_BUFFER_INFO info{};
            if (::GetConsoleScreenBufferInfo(outputHandle_, &info)) {
                const int width = static_cast<int>(info.srWindow.Right - info.srWindow.Left) + 1;
                const int height = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top) + 1;
                if (width > 0 && height > 0) {
                    cachedSize_ = TerminalSize{std::min(width, 1000), std::min(height, 500)};
                    return cachedSize_;
                }
            }
        }
        cachedSize_ = TerminalSize{std::max(20, env::getInt("COLUMNS", cachedSize_.width)),
                                   std::max(5, env::getInt("LINES", cachedSize_.height))};
        return cachedSize_;
    }

    void clear() override {
        if (virtualTerminal_) {
            write("\x1b[2J\x1b[H");
            return;
        }
        if (!outputHandle_) return;

        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!::GetConsoleScreenBufferInfo(outputHandle_, &info)) return;
        const DWORD cellCount =
            static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        const COORD home{0, 0};
        DWORD written = 0;
        ::FillConsoleOutputCharacterW(outputHandle_, L' ', cellCount, home, &written);
        ::FillConsoleOutputAttribute(outputHandle_, info.wAttributes, cellCount, home, &written);
        ::SetConsoleCursorPosition(outputHandle_, home);
    }

    void moveCursor(int x, int y) override {
        if (virtualTerminal_) {
            std::string sequence = "\x1b[";
            sequence.append(std::to_string(y + 1));
            sequence.push_back(';');
            sequence.append(std::to_string(x + 1));
            sequence.push_back('H');
            write(sequence);
            return;
        }
        if (outputHandle_) {
            const COORD position{static_cast<SHORT>(x), static_cast<SHORT>(y)};
            ::SetConsoleCursorPosition(outputHandle_, position);
        }
    }

    void showCursor(bool visible) override {
        if (virtualTerminal_) {
            write(visible ? "\x1b[?25h" : "\x1b[?25l");
            return;
        }
        if (!outputHandle_) return;
        CONSOLE_CURSOR_INFO info{};
        if (::GetConsoleCursorInfo(outputHandle_, &info)) {
            info.bVisible = visible ? TRUE : FALSE;
            ::SetConsoleCursorInfo(outputHandle_, &info);
        }
    }

    KeyEvent readKey() override {
        if (queue_.empty()) {
            drainEvents();
            if (queue_.empty() && inputHandle_) {
                // A short wait keeps the event loop from spinning while still
                // staying responsive; console handles wake on both key input
                // and buffer-size (resize) events.
                ::WaitForSingleObject(inputHandle_, kInputWaitMillis);
                drainEvents();
            }
        }
        if (queue_.empty()) return KeyEvent{};
        const KeyEvent event = queue_.front();
        queue_.pop_front();
        return event;
    }

    bool hasPendingInput() override {
        drainEvents();
        return !queue_.empty();
    }

    void write(std::string_view bytes) override {
        if (!outputHandle_ || bytes.empty()) return;
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const DWORD chunk = static_cast<DWORD>(
                std::min<std::size_t>(bytes.size() - offset, static_cast<std::size_t>(1) << 20));
            DWORD written = 0;
            if (!::WriteFile(outputHandle_, bytes.data() + offset, chunk, &written, nullptr) ||
                written == 0) {
                break;
            }
            offset += written;
        }
    }

    void flush() override {
        // Writes go straight to the console handle, so there is nothing to
        // flush.  Kept explicit so callers can express intent.
    }

    TerminalCapabilities capabilities() const override {
        TerminalCapabilities caps;
        caps.ansi = virtualTerminal_;
        caps.unicode = utf8_;
        caps.cursorAddressing = outputHandle_ != nullptr;
        caps.alternateScreen = virtualTerminal_;
        caps.colorMode = virtualTerminal_ ? detectColorMode() : ColorMode::None;
        return caps;
    }

private:
    static HANDLE NormaliseHandle(HANDLE handle) {
        return (handle == INVALID_HANDLE_VALUE || handle == nullptr) ? nullptr : handle;
    }

    void drainEvents() {
        if (!inputHandle_) return;

        DWORD available = 0;
        if (!::GetNumberOfConsoleInputEvents(inputHandle_, &available) || available == 0) return;

        const DWORD toRead = std::min<DWORD>(available, kMaxRecordsPerDrain);
        INPUT_RECORD records[kMaxRecordsPerDrain];
        DWORD read = 0;
        if (!::ReadConsoleInputW(inputHandle_, records, toRead, &read)) return;

        for (DWORD i = 0; i < read; ++i) {
            switch (records[i].EventType) {
                case KEY_EVENT:
                    handleKeyEvent(records[i].Event.KeyEvent);
                    break;
                case WINDOW_BUFFER_SIZE_EVENT:
                    // size() re-queries the console on demand, so nothing to do
                    // beyond letting the event wake the loop.
                    break;
                default:
                    break;  // mouse, focus and menu events are not used
            }
        }
    }

    void handleKeyEvent(const KEY_EVENT_RECORD& record) {
        if (!record.bKeyDown) return;  // key-up records carry no new information

        const unsigned mods = modifiersFromControlState(record.dwControlKeyState);
        const int repeats = std::clamp<int>(record.wRepeatCount, 1, 64);

        if (Key special = keyFromVirtualKey(record.wVirtualKeyCode); special != Key::None) {
            if (special == Key::Tab && (mods & ModShift) != 0) special = Key::BackTab;
            for (int i = 0; i < repeats; ++i) queue_.push_back(specialEvent(special, mods));
            return;
        }

        const wchar_t unit = record.uChar.UnicodeChar;
        if (unit == 0) return;  // modifier-only key (Shift, Ctrl, Alt, ...)

        if (unit < 0x20 || unit == 0x7F) {
            // Ctrl+C, Ctrl+D, Enter, Tab, Backspace, Escape and friends.  The
            // console reports them as control characters because processed
            // input is disabled.
            KeyEvent event = controlByteEvent(static_cast<unsigned char>(unit));
            if (event.key == Key::None) return;
            event.modifiers |= mods;
            for (int i = 0; i < repeats; ++i) queue_.push_back(event);
            return;
        }

        // UTF-16 surrogate pairs (emoji and other astral characters).
        char32_t codePoint = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            pendingHighSurrogate_ = unit;
            return;
        }
        if (unit >= 0xDC00 && unit <= 0xDFFF) {
            if (pendingHighSurrogate_ == 0) return;
            codePoint = 0x10000 + ((static_cast<char32_t>(pendingHighSurrogate_) - 0xD800) << 10) +
                        (static_cast<char32_t>(unit) - 0xDC00);
            pendingHighSurrogate_ = 0;
        } else {
            pendingHighSurrogate_ = 0;
        }

        for (int i = 0; i < repeats; ++i) queue_.push_back(characterEvent(codePoint, mods));
    }

    HANDLE outputHandle_ = nullptr;
    HANDLE inputHandle_ = nullptr;

    bool initialized_ = false;
    bool inputReady_ = false;
    bool inputIsConsole_ = false;
    bool outputIsConsole_ = false;
    bool virtualTerminal_ = false;
    bool utf8_ = false;

    DWORD originalOutputMode_ = 0;
    DWORD originalInputMode_ = 0;
    UINT originalOutputCodePage_ = 0;
    UINT originalInputCodePage_ = 0;

    wchar_t pendingHighSurrogate_ = 0;
    std::deque<KeyEvent> queue_;
    mutable TerminalSize cachedSize_{80, 24};
};

}  // namespace

std::unique_ptr<Terminal> createTerminal() { return std::make_unique<WindowsTerminal>(); }

}  // namespace mdview::terminal
