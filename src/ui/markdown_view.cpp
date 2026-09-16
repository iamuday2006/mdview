#include "ui/markdown_view.hpp"

#include "utils/unicode.hpp"

#include <algorithm>
#include <vector>

namespace mdview::ui {

namespace {

constexpr int kScrollbarColumns = 1;

struct VisibleLine {
    int row = 0;
    std::size_t index = 0;
};

/// Maps rows of `area` to rendered-line indices, honouring the scroll offset.
std::vector<VisibleLine> visibleLines(const Rect& area, const markdown::RenderedDocument& document,
                                      const MarkdownViewState& state) {
    std::vector<VisibleLine> result;
    if (area.height <= 0) return result;
    const std::size_t first = std::min(state.scroll, document.lines.size());
    for (int row = 0; row < area.height; ++row) {
        const std::size_t index = first + static_cast<std::size_t>(row);
        if (index >= document.lines.size()) break;
        result.push_back(VisibleLine{row, index});
    }
    return result;
}

}  // namespace

int widestLine(const markdown::RenderedDocument& document) {
    int widest = 0;
    for (const markdown::RenderedLine& line : document.lines) {
        widest = std::max(widest, line.displayWidth());
    }
    return widest;
}

void clampMarkdownScroll(MarkdownViewState& state, const markdown::RenderedDocument& document,
                         const Rect& area, int contentWidth, bool wrap) {
    const std::size_t rows = area.height > 0 ? static_cast<std::size_t>(area.height) : 0;
    const std::size_t total = document.lines.size();
    const std::size_t maxScroll = total > rows ? total - rows : 0;
    state.scroll = std::min(state.scroll, maxScroll);

    if (wrap) {
        state.horizontalScroll = 0;
        return;
    }
    const int overflow = std::max(0, widestLine(document) - contentWidth);
    state.horizontalScroll = std::clamp(state.horizontalScroll, 0, overflow);
}

void drawMarkdownView(terminal::Screen& screen, const Rect& area,
                      const markdown::RenderedDocument& document, const MarkdownViewState& state,
                      const terminal::Theme& theme,
                      const terminal::TerminalCapabilities& capabilities, bool focused,
                      const std::string& emptyMessage) {
    if (area.empty()) return;

    screen.pushClip(area);
    screen.fillRect(area, U' ', theme.text);

    if (document.lines.empty()) {
        if (!emptyMessage.empty() && area.height > 0) {
            screen.drawText(area.x + 1, area.y, emptyMessage, theme.muted);
        }
        screen.popClip();
        return;
    }

    const int rows = area.height;
    const int textWidth = std::max(1, area.width - kScrollbarColumns);
    const std::vector<VisibleLine> rowsToDraw = visibleLines(area, document, state);

    for (const VisibleLine& visible : rowsToDraw) {
        int x = area.x - state.horizontalScroll;
        for (const markdown::Span& span : document.lines[visible.index].spans) {
            if (x >= area.x + textWidth) break;
            screen.drawText(x, area.y + visible.row, span.text, span.style);
            x += uni::displayWidth(span.text);
        }
    }

    // --- scrollbar ---------------------------------------------------------
    if (capabilities.cursorAddressing && document.lines.size() > static_cast<std::size_t>(rows) &&
        area.width > 4) {
        const int barX = area.right() - kScrollbarColumns;
        const std::string_view track = capabilities.unicode ? "\xE2\x94\x82" : "|";
        const std::string_view thumb = capabilities.unicode ? "\xE2\x94\x83" : "#";

        const int total = static_cast<int>(document.lines.size());
        int thumbHeight = std::max(1, rows * rows / std::max(1, total));
        thumbHeight = std::min(thumbHeight, rows);
        const int maxScroll = std::max(1, total - rows);
        const int travel = rows - thumbHeight;
        const int thumbTop =
            area.y + static_cast<int>(static_cast<long long>(travel) * static_cast<long long>(state.scroll) /
                                      maxScroll);

        for (int row = 0; row < rows; ++row) {
            const int y = area.y + row;
            const bool inThumb = row >= thumbTop - area.y && row < thumbTop - area.y + thumbHeight;
            screen.drawText(barX, y, inThumb ? thumb : track, inThumb ? theme.scrollbarThumb : theme.scrollbar);
        }
        (void)focused;
    }

    screen.popClip();
}

void paintSearchHighlights(terminal::Screen& screen, const Rect& area,
                           const markdown::RenderedDocument& document,
                           const MarkdownViewState& state, const search::SearchState& search,
                           const terminal::Theme& theme) {
    if (area.empty() || !search.hasMatches()) return;

    const int textWidth = std::max(1, area.width - kScrollbarColumns);
    const int viewStart = area.x - state.horizontalScroll;
    const int viewEnd = viewStart + textWidth;
    const std::size_t first = std::min(state.scroll, document.lines.size());

    // Restyles one match in place.  Column arithmetic happens in display
    // columns (a CJK glyph is two), but bytes are what get re-drawn, so the
    // final cut goes through byteRangeForColumns() to stay on code-point
    // boundaries.  Clipping against the view keeps a half-scrolled match from
    // painting over the scrollbar or outside the pane.
    const auto paintMatch = [&](const search::SearchMatch& match, bool current) {
        if (match.line < first) return;
        const std::size_t row = match.line - first;
        if (row >= static_cast<std::size_t>(area.height)) return;

        const int y = area.y + static_cast<int>(row);
        if (match.endColumn <= viewStart || match.startColumn >= viewEnd) return;

        const markdown::RenderedLine& line = document.lines[match.line];
        const terminal::Style& overlay = current ? theme.searchMatchCurrent : theme.searchMatch;

        int spanStart = 0;
        for (const markdown::Span& span : line.spans) {
            const int spanEnd = spanStart + uni::displayWidth(span.text);
            const int overlapStart = std::max({spanStart, match.startColumn, viewStart});
            const int overlapEnd = std::min({spanEnd, match.endColumn, viewEnd});
            if (overlapStart < overlapEnd) {
                const search::SearchByteRange range = search::byteRangeForColumns(
                    span.text, overlapStart - spanStart, overlapEnd - spanStart);
                if (range.end > range.start) {
                    terminal::Style style = overlay;
                    style.underline = style.underline || span.style.underline;
                    const int x = viewStart + overlapStart;
                    screen.drawText(x, y, span.text.substr(range.start, range.end - range.start), style);
                }
            }
            spanStart = spanEnd;
            if (spanStart >= match.endColumn && spanStart >= viewEnd) break;
        }
    };

    for (std::size_t i = 0; i < search.matches.size(); ++i) {
        if (i != search.current) paintMatch(search.matches[i], false);
    }
    if (search.current < search.matches.size()) {
        paintMatch(search.matches[search.current], true);
    }
}

}  // namespace mdview::ui
