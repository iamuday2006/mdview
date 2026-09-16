#include "terminal/screen.hpp"

#include "terminal/terminal.hpp"
#include "utils/unicode.hpp"

#include <algorithm>

namespace mdview::terminal {

namespace {

constexpr BoxChars kBoxNone{"", "", "", "", "", "", "", "", "", "", ""};
constexpr BoxChars kBoxLight{"\u250C", "\u2510", "\u2514", "\u2518", "\u2500", "\u2502",
                             "\u251C", "\u2524", "\u252C", "\u2534", "\u253C"};
constexpr BoxChars kBoxRounded{"\u256D", "\u256E", "\u2570", "\u256F", "\u2500", "\u2502",
                               "\u251C", "\u2524", "\u252C", "\u2534", "\u253C"};
constexpr BoxChars kBoxHeavy{"\u250F", "\u2513", "\u2517", "\u251B", "\u2501", "\u2503",
                             "\u2523", "\u252B", "\u2533", "\u253B", "\u254B"};
constexpr BoxChars kBoxDouble{"\u2554", "\u2557", "\u255A", "\u255D", "\u2550", "\u2551",
                              "\u2560", "\u2563", "\u2566", "\u2569", "\u256C"};
constexpr BoxChars kBoxAscii{"+", "+", "+", "+", "-", "|", "+", "+", "+", "+", "+"};

}  // namespace

const BoxChars& boxCharsFor(BorderStyle style) {
    switch (style) {
        case BorderStyle::Light:
            return kBoxLight;
        case BorderStyle::Rounded:
            return kBoxRounded;
        case BorderStyle::Heavy:
            return kBoxHeavy;
        case BorderStyle::Double:
            return kBoxDouble;
        case BorderStyle::Ascii:
            return kBoxAscii;
        case BorderStyle::None:
            break;
    }
    return kBoxNone;
}

void Screen::resize(int width, int height) {
    width = std::max(0, width);
    height = std::max(0, height);
    if (width == width_ && height == height_) return;

    width_ = width;
    height_ = height;

    const std::size_t count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    cells_.assign(count, Cell{});
    flags_.assign(count, Normal);
    previous_.assign(count, Cell{});
    previousFlags_.assign(count, Normal);

    clips_.clear();
    clip_ = Rect{0, 0, width_, height_};
    forceFull_ = true;
}

void Screen::clear(const Style& style) {
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        cells_[i] = Cell{U' ', style};
        flags_[i] = Normal;
    }
}

void Screen::erasePartner(int x, int y) {
    const std::size_t idx = index(x, y);
    if (flags_[idx] == WideLead) {
        const int nx = x + 1;
        if (nx < width_) {
            cells_[index(nx, y)] = Cell{};
            flags_[index(nx, y)] = Normal;
        }
    } else if (flags_[idx] == WideTail) {
        const int px = x - 1;
        if (px >= 0) {
            cells_[index(px, y)] = Cell{};
            flags_[index(px, y)] = Normal;
        }
    }
    flags_[idx] = Normal;
}

void Screen::resetCell(int x, int y) {
    const std::size_t idx = index(x, y);
    cells_[idx] = Cell{};
    flags_[idx] = Normal;
}

void Screen::putCell(int x, int y, char32_t ch, const Style& style) {
    const int glyphWidth = uni::codepointWidth(ch);
    if (glyphWidth <= 0) {
        // Combining marks and format characters have no advance of their own
        // and cannot be represented in a one-code-point-per-cell model.
        return;
    }

    const bool wide = glyphWidth == 2;
    if (wide && !(x + 1 < width_ && clip_.contains(x + 1, y))) {
        // There is no room for the whole glyph, so draw a blank rather than
        // half a character, which would desynchronise the terminal's cursor
        // from our grid.
        erasePartner(x, y);
        const std::size_t idx = index(x, y);
        cells_[idx] = Cell{U' ', style};
        flags_[idx] = Normal;
        return;
    }

    const std::size_t idx = index(x, y);
    erasePartner(x, y);
    cells_[idx] = Cell{ch, style};
    flags_[idx] = wide ? WideLead : Normal;

    if (wide) {
        const std::size_t tail = index(x + 1, y);
        erasePartner(x + 1, y);
        cells_[tail] = Cell{U' ', style};
        flags_[tail] = WideTail;
    }
}

void Screen::setCell(int x, int y, char32_t ch, const Style& style) {
    if (!clip_.contains(x, y)) return;
    putCell(x, y, ch, style);
}

void Screen::drawText(int x, int y, std::string_view text, const Style& style) {
    if (y < clip_.y || y >= clip_.bottom()) return;

    int cursor = x;
    std::size_t i = 0;
    while (i < text.size()) {
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(text, i, codePoint);
        if (consumed == 0) break;
        i += consumed;

        const int glyphWidth = uni::codepointWidth(codePoint);
        if (glyphWidth == 0) continue;
        if (cursor >= clip_.right()) break;
        if (cursor >= clip_.x && clip_.contains(cursor, y)) putCell(cursor, y, codePoint, style);
        cursor += glyphWidth;
    }
}

void Screen::fillRect(const Rect& area, char32_t ch, const Style& style) {
    const Rect target = area.intersected(clip_);
    if (target.empty()) return;
    for (int y = target.y; y < target.bottom(); ++y) {
        for (int x = target.x; x < target.right(); ++x) {
            putCell(x, y, ch, style);
        }
    }
}

void Screen::drawHLine(int x, int y, int length, std::string_view glyph, const Style& style) {
    if (length <= 0) return;
    if (y < clip_.y || y >= clip_.bottom()) return;
    const int glyphWidth = std::max(1, uni::displayWidth(glyph));
    int cursor = x;
    while (cursor < x + length) {
        if (cursor >= clip_.right()) break;
        if (cursor >= clip_.x) drawText(cursor, y, glyph, style);
        cursor += glyphWidth;
    }
}

void Screen::drawVLine(int x, int y, int length, std::string_view glyph, const Style& style) {
    if (length <= 0 || x < clip_.x || x >= clip_.right()) return;
    for (int row = y; row < y + length; ++row) {
        if (row < clip_.y) continue;
        if (row >= clip_.bottom()) break;
        drawText(x, row, glyph, style);
    }
}

void Screen::drawBox(const Rect& area, BorderStyle border, const Style& style) {
    const BoxChars& chars = boxCharsFor(border);
    if (border == BorderStyle::None) return;
    if (area.width < 2 || area.height < 2) return;

    drawHLine(area.x + 1, area.y, area.width - 2, chars.horizontal, style);
    drawHLine(area.x + 1, area.y + area.height - 1, area.width - 2, chars.horizontal, style);
    drawVLine(area.x, area.y + 1, area.height - 2, chars.vertical, style);
    drawVLine(area.x + area.width - 1, area.y + 1, area.height - 2, chars.vertical, style);

    drawText(area.x, area.y, chars.topLeft, style);
    drawText(area.x + area.width - 1, area.y, chars.topRight, style);
    drawText(area.x, area.y + area.height - 1, chars.bottomLeft, style);
    drawText(area.x + area.width - 1, area.y + area.height - 1, chars.bottomRight, style);
}

void Screen::drawFrame(const Rect& area, BorderStyle border, const Style& style, std::string_view title,
                       const Style& titleStyle) {
    drawBox(area, border, style);
    if (title.empty() || area.width < 6) return;

    const int available = area.width - 5;
    const std::string label = uni::truncate(title, available, "\xE2\x80\xA6");
    const int labelWidth = uni::displayWidth(label);
    const int startX = area.x + 2;
    drawText(startX, area.y, " " + label + " ", titleStyle);

    const int titleEnd = startX + labelWidth + 2;
    drawHLine(titleEnd, area.y, area.right() - 1 - titleEnd, boxCharsFor(border).horizontal, style);
}

void Screen::pushClip(const Rect& area) {
    clips_.push_back(clip_);
    clip_ = clip_.intersected(area);
}

void Screen::popClip() {
    if (!clips_.empty()) {
        clip_ = clips_.back();
        clips_.pop_back();
    }
}

bool Screen::cellChanged(std::size_t idx) const {
    if (forceFull_) return true;
    if (flags_[idx] != previousFlags_[idx]) return true;
    return cells_[idx] != previous_[idx];
}

void Screen::flush(Terminal& terminal) {
    if (width_ <= 0 || height_ <= 0) return;

    const TerminalCapabilities capabilities = terminal.capabilities();
    const ColorMode mode = capabilities.colorMode;
    const bool useAnsi = capabilities.ansi;

    std::string out;
    out.reserve(4096);

    Style activeStyle;
    bool styleKnown = false;

    const auto emitCursor = [&](int column, int row) {
        if (useAnsi) {
            out.append("\x1b[");
            out.append(std::to_string(row + 1));
            out.push_back(';');
            out.append(std::to_string(column + 1));
            out.push_back('H');
        }
    };

    for (int y = 0; y < height_; ++y) {
        int x = 0;
        while (x < width_) {
            const std::size_t idx = index(x, y);
            if (!cellChanged(idx)) {
                ++x;
                continue;
            }

            const int runStartX = x;
            const Style style = cells_[idx].style;
            std::string run;
            while (x < width_) {
                const std::size_t current = index(x, y);
                if (!cellChanged(current) || !(cells_[current].style == style)) break;
                if (flags_[current] != WideTail) uni::appendUtf8(cells_[current].ch, run);
                ++x;
            }

            if (run.empty()) continue;

            if (useAnsi) {
                emitCursor(runStartX, y);
                if (!styleKnown || !(style == activeStyle)) {
                    out.append(sgrFor(style, mode));
                    activeStyle = style;
                    styleKnown = true;
                }
                out.append(run);
            } else {
                terminal.moveCursor(runStartX, y);
                terminal.write(run);
            }
        }
    }

    if (useAnsi && !out.empty() && mode != ColorMode::None) {
        out.append("\x1b[0m");
    }

    if (!out.empty()) terminal.write(out);

    previous_ = cells_;
    previousFlags_ = flags_;
    forceFull_ = false;
    terminal.flush();
}

const Cell& Screen::at(int x, int y) const {
    static const Cell blank{};
    if (!inBounds(x, y)) return blank;
    return cells_[index(x, y)];
}

std::string Screen::toText() const {
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(height_));
    for (int y = 0; y < height_; ++y) {
        std::string line;
        for (int x = 0; x < width_; ++x) {
            if (flags_[index(x, y)] == WideTail) continue;
            uni::appendUtf8(cells_[index(x, y)].ch, line);
        }
        // Drop trailing blanks so the output is stable in tests and readable
        // when piped to a file.
        std::size_t end = line.size();
        while (end > 0 && line[end - 1] == ' ') --end;
        line.resize(end);
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();

    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) out.push_back('\n');
        out.append(lines[i]);
    }
    return out;
}

std::string Screen::toAnsiText(ColorMode mode) const {
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(height_));
    for (int y = 0; y < height_; ++y) {
        std::string line;
        Style active;
        bool styleKnown = false;
        for (int x = 0; x < width_; ++x) {
            const Cell& cell = cells_[index(x, y)];
            if (flags_[index(x, y)] == WideTail) continue;
            if (mode != ColorMode::None && (!styleKnown || !(cell.style == active))) {
                line.append(sgrFor(cell.style, mode));
                active = cell.style;
                styleKnown = true;
            }
            uni::appendUtf8(cell.ch, line);
        }
        // Trim trailing blanks before closing the line so piped output stays
        // clean; a dangling SGR sequence at the end is harmless.
        std::size_t end = line.size();
        while (end > 0 && line[end - 1] == ' ') --end;
        line.resize(end);
        if (mode != ColorMode::None) line.append("\x1b[0m");
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back() == "\x1b[0m") lines.pop_back();

    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) out.push_back('\n');
        out.append(lines[i]);
    }
    return out;
}

}  // namespace mdview::terminal
