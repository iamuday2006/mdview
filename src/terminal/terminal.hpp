#pragma once

#include "terminal/input.hpp"
#include "terminal/style.hpp"

#include <cstddef>
#include <deque>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

// The single seam between the portable application and the operating system.
//
// Everything above this interface -- Markdown parsing, rendering, the file
// tree, the widgets and the event loop -- is written once and never asks which
// platform it is running on.  Everything below it (console modes on Windows,
// termios and signal handling on POSIX) lives in src/platform/.

namespace mdview::terminal {

struct TerminalSize {
    int width = 80;
    int height = 24;
};

struct TerminalCapabilities {
    /// Escape sequences are understood (VT processing / xterm-compatible).
    bool ansi = false;
    /// The font and locale can be assumed to render box drawing and glyphs.
    bool unicode = true;
    /// Cursor addressing through moveCursor() works.
    bool cursorAddressing = false;
    /// The alternate screen buffer is available.
    bool alternateScreen = false;
    ColorMode colorMode = ColorMode::None;

    bool colored() const { return colorMode != ColorMode::None; }
};

class Terminal {
public:
    virtual ~Terminal();

    /// Puts the terminal into raw/alternate-screen mode.  Safe to call twice;
    /// shutdown() restores exactly what was saved.
    virtual void initialize() = 0;
    virtual void shutdown() = 0;

    /// True when the terminal is a real interactive terminal (both standard
    /// streams attached).  A false result means the caller should fall back to
    /// non-interactive output instead of trying to run the TUI.
    virtual bool isInteractive() const = 0;

    virtual TerminalSize size() const = 0;

    virtual void clear() = 0;
    virtual void moveCursor(int x, int y) = 0;
    virtual void showCursor(bool visible) = 0;

    /// Returns the next key, or Key::None when nothing is available.  The call
    /// may block for a very short moment (a few milliseconds) so the event
    /// loop does not spin; it never blocks for user-perceivable time.
    virtual KeyEvent readKey() = 0;
    virtual bool hasPendingInput() = 0;

    /// Writes bytes (usually a batch of escape sequences) to the terminal.
    virtual void write(std::string_view bytes) = 0;
    virtual void flush() = 0;

    virtual TerminalCapabilities capabilities() const = 0;
};

/// Creates the Terminal implementation for the platform this binary was built
/// for.  Implemented in src/platform/<os>/.
std::unique_ptr<Terminal> createTerminal();

/// A Terminal that plays back scripted input and records everything written.
/// It needs no platform code at all, which makes the whole UI testable and
/// gives the application a usable fallback when there is no TTY.
class HeadlessTerminal final : public Terminal {
public:
    explicit HeadlessTerminal(TerminalSize size = TerminalSize{100, 30});

    void initialize() override;
    void shutdown() override;
    bool isInteractive() const override { return false; }

    TerminalSize size() const override { return size_; }
    void setSize(TerminalSize size) { size_ = size; }

    void clear() override;
    void moveCursor(int x, int y) override;
    void showCursor(bool visible) override;

    KeyEvent readKey() override;
    bool hasPendingInput() override;

    void write(std::string_view bytes) override;
    void flush() override;

    TerminalCapabilities capabilities() const override;

    // --- Test helpers ----------------------------------------------------
    void pushInput(const KeyEvent& event);
    void pushInput(std::initializer_list<KeyEvent> events);
    void pushText(std::string_view text);
    void pushBytes(std::string_view bytes);

    const std::string& output() const { return output_; }
    void clearOutput() { output_.clear(); }

    void setCapabilities(TerminalCapabilities capabilities) { capabilities_ = capabilities; }
    /// Records cursor positions passed to moveCursor() so tests can assert on
    /// the non-ANSI rendering path.
    const std::deque<std::pair<int, int>>& cursorMoves() const { return cursorMoves_; }

private:
    TerminalSize size_;
    TerminalCapabilities capabilities_;
    std::deque<KeyEvent> input_;
    std::string output_;
    std::deque<std::pair<int, int>> cursorMoves_;
};

}  // namespace mdview::terminal
