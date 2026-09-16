#pragma once

#include "filesystem/file_tree.hpp"
#include "terminal/screen.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"
#include "utils/geometry.hpp"

#include <cstddef>

namespace mdview::ui {

struct TreeViewState {
    std::size_t selected = 0;
    std::size_t scroll = 0;
};

/// Draws the visible part of the file tree inside `area`.
void drawTreeView(terminal::Screen& screen, const Rect& area, const filesystem::FileTree& tree,
                  const TreeViewState& state, const terminal::Theme& theme,
                  const terminal::TerminalCapabilities& capabilities, bool focused);

/// Keeps the selected row inside a viewport of `rows` rows.
void scrollTreeToSelection(TreeViewState& state, std::size_t rowCount, int rows);

}  // namespace mdview::ui
