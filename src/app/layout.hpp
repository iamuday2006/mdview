#pragma once

#include "utils/geometry.hpp"

// Frame layout.
//
// The whole is a pure function of the screen rectangle and a couple of
// preferences, which means the resize path is just "recompute, then redraw" and
// the geometry is trivially unit-testable.

namespace mdview::app {

struct Layout {
    Rect header;
    Rect body;
    Rect tree;
    Rect divider;
    Rect content;
    Rect status;
};

struct LayoutOptions {
    bool showTree = true;
    /// 0 selects an automatic width derived from the screen.
    int treeWidth = 0;
    /// Minimum body width for the tree pane to appear at all.
    int minimumBodyWidthForTree = 44;
};

Layout computeLayout(const Rect& screen, const LayoutOptions& options);

}  // namespace mdview::app
