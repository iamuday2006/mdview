#pragma once

#include "terminal/style.hpp"
#include "utils/geometry.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// A platform-neutral cell grid.
//
// Widgets draw into the Screen; the Screen decides how to turn that into bytes
// for the terminal.  Two things make this cheap and stable:
//
//   * the application redraws the whole scene every frame, so widgets never
//     have to worry about stale cells;
//   * flush() diffs against the previous frame and only emits changed runs,
//     so redrawing everything costs nothing on the wire.
//
// Nothing in here is OS specific: the only escape hatch to the platform is the
// abstract Terminal passed to flush().

namespace mdview::terminal {

class Terminal;

struct Cell {
    char32_t ch = U' ';
    Style style;

    friend bool operator==(const Cell&, const Cell&) noexcept = default;
};

enum class BorderStyle : std::uint8_t { None, Light, Rounded, Heavy, Double, Ascii };

struct BoxChars {
    std::string_view topLeft;
    std::string_view topRight;
    std::string_view bottomLeft;
    std::string_view bottomRight;
    std::string_view horizontal;
    std::string_view vertical;
    std::string_view teeLeft;
    std::string_view teeRight;
    std::string_view teeTop;
    std::string_view teeBottom;
    std::string_view cross;
};

const BoxChars& boxCharsFor(BorderStyle style);

class Screen {
public:
    Screen() = default;

    void resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    Rect bounds() const { return Rect{0, 0, width_, height_}; }

    /// Fills the whole grid with a blank cell using `style` as its background.
    void clear(const Style& style = {});

    // --- Drawing ---------------------------------------------------------
    void setCell(int x, int y, char32_t ch, const Style& style);
    void drawText(int x, int y, std::string_view text, const Style& style);
    void fillRect(const Rect& area, char32_t ch, const Style& style);
    void drawHLine(int x, int y, int length, std::string_view glyph, const Style& style);
    void drawVLine(int x, int y, int length, std::string_view glyph, const Style& style);
    void drawBox(const Rect& area, BorderStyle border, const Style& style);
    /// Frame with an embedded title, e.g. "┌─ README.md ─────┐".
    void drawFrame(const Rect& area, BorderStyle border, const Style& style, std::string_view title,
                   const Style& titleStyle);

    /// Restricts drawing to the intersection of `area` and the current clip.
    void pushClip(const Rect& area);
    void popClip();
    Rect clip() const { return clip_; }

    // --- Output ----------------------------------------------------------
    /// Forces the next flush() to repaint every cell.
    void invalidate() { forceFull_ = true; }
    /// Emits the difference between the current and previous frame.
    void flush(Terminal& terminal);

    // --- Inspection (tests, --print fallback) ----------------------------
    const Cell& at(int x, int y) const;
    /// The grid as plain text, one line per row with trailing blanks removed.
    std::string toText() const;
    /// The grid with SGR sequences applied per run (used by --print).
    std::string toAnsiText(ColorMode mode) const;

private:
    enum Flags : std::uint8_t { Normal = 0, WideLead = 1, WideTail = 2 };

    std::size_t index(int x, int y) const { return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x); }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    void putCell(int x, int y, char32_t ch, const Style& style);
    void erasePartner(int x, int y);
    void resetCell(int x, int y);
    bool cellChanged(std::size_t idx) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
    std::vector<std::uint8_t> flags_;

    std::vector<Cell> previous_;
    std::vector<std::uint8_t> previousFlags_;
    bool forceFull_ = true;

    std::vector<Rect> clips_;
    Rect clip_{};
};

}  // namespace mdview::terminal
