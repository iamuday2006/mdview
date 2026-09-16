#pragma once

#include "markdown/renderer.hpp"
#include "search/search.hpp"
#include "terminal/screen.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"
#include "utils/geometry.hpp"

#include <cstddef>
#include <string>

namespace mdview::ui {

struct MarkdownViewState {
    std::size_t scroll = 0;
    int horizontalScroll = 0;
};

/// Blits the visible slice of an already-rendered document into `area`.
/// The widget is deliberately dumb: the renderer did all width-dependent work,
/// so resizing just means re-rendering and calling this again.
void drawMarkdownView(terminal::Screen& screen, const Rect& area,
                      const markdown::RenderedDocument& document, const MarkdownViewState& state,
                      const terminal::Theme& theme,
                      const terminal::TerminalCapabilities& capabilities, bool focused,
                      const std::string& emptyMessage);

/// Re-styles the cells under `matches` after the view has been drawn.  Runs on
/// top of the blit, so highlighting never has to touch the draw loop: a line
/// with no match is untouched, and a match that scrolled out is clipped away.
/// The current match (if any) is styled after the others so it wins.
void paintSearchHighlights(terminal::Screen& screen, const Rect& area,
                           const markdown::RenderedDocument& document,
                           const MarkdownViewState& state, const search::SearchState& search,
                           const terminal::Theme& theme);

/// Clamps scroll offsets after a resize, a reload or a wrap change.
void clampMarkdownScroll(MarkdownViewState& state, const markdown::RenderedDocument& document,
                         const Rect& area, int contentWidth, bool wrap);

/// Widest line in the document, used for horizontal (no-wrap) scrolling.
int widestLine(const markdown::RenderedDocument& document);

}  // namespace mdview::ui
