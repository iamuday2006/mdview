#include "terminal/terminal.hpp"

#include "utils/env.hpp"
#include "utils/string_utils.hpp"

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <string>

// POSIX terminal implementation, shared by the Linux and macOS builds: both
// expose exactly the same termios / ioctl / SIGWINCH interface, so splitting
// them into two byte-identical files would be duplication without benefit.
// src/platform/linux/ and src/platform/macos/ own the differences that do
// exist (platform identity, package-specific quirks).

namespace mdview::terminal {

namespace {

constexpr int kInputPollMillis = 16;

/// Set from the signal handler, which is why it must be a plain sig_atomic_t
/// rather than anything that could allocate or lock.
volatile std::sig_atomic_t g_resized = 0;

extern "C" void handleWindowChange(int) { g_resized = 1; }

bool localeLooksUtf8() {
    for (const char* variable : {"LC_ALL", "LC_CTYPE", "LANG"}) {
        if (const auto value = env::get(variable)) {
            if (value->find("UTF-8") != std::string::npos ||
                value->find("utf-8") != std::string::npos ||
                value->find("UTF8") != std::string::npos ||
                value->find("utf8") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

ColorMode detectColorMode() {
    if (env::get("NO_COLOR")) return ColorMode::None;

    if (const auto colorTerm = env::get("COLORTERM")) {
        if (str::equalsIgnoreCaseAscii(*colorTerm, "truecolor") ||
            str::equalsIgnoreCaseAscii(*colorTerm, "24bit")) {
            return ColorMode::TrueColor;
        }
    }
    const auto term = env::get("TERM");
    if (term) {
        if (term->find("dumb") != std::string::npos) return ColorMode::None;
        if (term->find("direct") != std::string::npos || term->find("truecolor") != std::string::npos) {
            return ColorMode::TrueColor;
        }
        if (term->find("256color") != std::string::npos) return ColorMode::Ansi256;
    }
    return ColorMode::Basic;
}

class PosixTerminal final : public Terminal {
public:
    PosixTerminal() = default;

    ~PosixTerminal() override { shutdown(); }

    void initialize() override {
        if (initialized_) return;
        initialized_ = true;

        inputIsTerminal_ = ::isatty(STDIN_FILENO) != 0;
        outputIsTerminal_ = ::isatty(STDOUT_FILENO) != 0;

        if (inputIsTerminal_ && ::tcgetattr(STDIN_FILENO, &originalTermios_) == 0) {
            haveOriginalTermios_ = true;
            struct termios raw = originalTermios_;
            // Classic raw mode: no echo, no line buffering, no signal
            // generation (so Ctrl+C reaches us as a byte) and no IXON (so
            // Ctrl+S / Ctrl+Q are ours too).
            raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
            raw.c_oflag &= static_cast<tcflag_t>(~(OPOST));
            raw.c_cflag |= static_cast<tcflag_t>(CS8);
            raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
                rawMode_ = true;
            }
        }

        // Resize notification: SIGWINCH simply sets a flag; the flag is read in
        // size(), which is the only place that needs to know.
        struct sigaction action{};
        action.sa_handler = handleWindowChange;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;  // no SA_RESTART: we want poll() to be interrupted
        if (::sigaction(SIGWINCH, &action, &previousWinchAction_) == 0) {
            havePreviousWinch_ = true;
        }

        capabilities_.ansi = outputIsTerminal_;
        capabilities_.unicode = localeLooksUtf8();
        capabilities_.cursorAddressing = outputIsTerminal_;
        capabilities_.alternateScreen = outputIsTerminal_;
        capabilities_.colorMode = outputIsTerminal_ ? detectColorMode() : ColorMode::None;

        if (capabilities_.ansi) {
            write("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H");
        }
        refreshSize();
    }

    void shutdown() override {
        if (!initialized_) return;
        initialized_ = false;

        if (capabilities_.ansi) {
            write("\x1b[0m\x1b[?25h\x1b[?1049l");
        }
        if (rawMode_ && haveOriginalTermios_) {
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
            rawMode_ = false;
        }
        if (havePreviousWinch_) {
            ::sigaction(SIGWINCH, &previousWinchAction_, nullptr);
            havePreviousWinch_ = false;
        }
    }

    bool isInteractive() const override { return inputIsTerminal_ && outputIsTerminal_ && rawMode_; }

    TerminalSize size() const override {
        if (g_resized != 0) {
            g_resized = 0;
            refreshSize();
        }
        return cachedSize_;
    }

    void clear() override { write("\x1b[2J\x1b[H"); }

    void moveCursor(int x, int y) override {
        std::string sequence = "\x1b[";
        sequence.append(std::to_string(y + 1));
        sequence.push_back(';');
        sequence.append(std::to_string(x + 1));
        sequence.push_back('H');
        write(sequence);
    }

    void showCursor(bool visible) override { write(visible ? "\x1b[?25h" : "\x1b[?25l"); }

    KeyEvent readKey() override {
        if (queue_.empty()) {
            pump(kInputPollMillis);
            if (queue_.empty() && decoder_.pending()) {
                // Nothing followed a partial sequence (usually a bare Escape),
                // so interpret what we have instead of waiting forever.
                for (const KeyEvent& event : decoder_.flush()) queue_.push_back(event);
            }
        }
        if (queue_.empty()) return KeyEvent{};
        const KeyEvent event = queue_.front();
        queue_.pop_front();
        return event;
    }

    bool hasPendingInput() override {
        if (queue_.empty()) {
            pump(0);
            if (queue_.empty() && decoder_.pending()) {
                for (const KeyEvent& event : decoder_.flush()) queue_.push_back(event);
            }
        }
        return !queue_.empty();
    }

    void write(std::string_view bytes) override {
        if (bytes.empty()) return;
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t written = ::write(STDOUT_FILENO, bytes.data() + offset, bytes.size() - offset);
            if (written <= 0) {
                if (errno == EINTR) continue;
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
    }

    void flush() override {
        // ::write is unbuffered, so there is nothing to flush.
    }

    TerminalCapabilities capabilities() const override { return capabilities_; }

private:
    void refreshSize() const {
        struct winsize window{};
        if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 && window.ws_row > 0) {
            cachedSize_ = TerminalSize{std::min<int>(window.ws_col, 1000), std::min<int>(window.ws_row, 500)};
            return;
        }
        cachedSize_ = TerminalSize{std::max(20, env::getInt("COLUMNS", cachedSize_.width)),
                                   std::max(5, env::getInt("LINES", cachedSize_.height))};
    }

    void pump(int timeoutMillis) {
        struct pollfd descriptor{};
        descriptor.fd = STDIN_FILENO;
        descriptor.events = POLLIN;

        const int ready = ::poll(&descriptor, 1, timeoutMillis);
        if (ready <= 0) return;  // timeout or EINTR (e.g. from SIGWINCH)

        char buffer[1024];
        const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count > 0) {
            for (const KeyEvent& event :
                 decoder_.feed(std::string_view(buffer, static_cast<std::size_t>(count)))) {
                queue_.push_back(event);
            }
            return;
        }
        if (count == 0) {
            // stdin closed (piped input ran out).  Hand the application a
            // Ctrl+D so it can shut down cleanly instead of spinning.
            queue_.push_back(specialEvent(Key::CtrlD, ModCtrl));
        }
    }

    mutable TerminalSize cachedSize_{80, 24};

    TerminalCapabilities capabilities_;
    bool initialized_ = false;
    bool inputIsTerminal_ = false;
    bool outputIsTerminal_ = false;
    bool rawMode_ = false;
    bool haveOriginalTermios_ = false;
    struct termios originalTermios_{};

    bool havePreviousWinch_ = false;
    struct sigaction previousWinchAction_{};

    InputDecoder decoder_;
    std::deque<KeyEvent> queue_;
};

}  // namespace

std::unique_ptr<Terminal> createTerminal() { return std::make_unique<PosixTerminal>(); }

}  // namespace mdview::terminal
