#include "app/layout.hpp"

#include <algorithm>

namespace mdview::app {

Layout computeLayout(const Rect& screen, const LayoutOptions& options) {
    Layout layout;
    Rect area = screen;

    // A header and a status bar are always shown; they are the cheapest way to
    // keep the user oriented and are never worth hiding.
    layout.header = carveTop(area, 1);
    layout.status = carveBottom(area, area.height > 2 ? 1 : 0);
    layout.body = area;

    const bool wantTree = options.showTree && area.width >= options.minimumBodyWidthForTree &&
                          area.height >= 4;
    if (!wantTree) {
        layout.content = area;
        return layout;
    }

    int treeWidth = options.treeWidth;
    if (treeWidth <= 0) {
        treeWidth = std::clamp(area.width / 4, 20, 40);
    }
    treeWidth = std::clamp(treeWidth, 12, std::max(12, area.width - 24));

    layout.tree = carveLeft(area, treeWidth);
    layout.divider = carveLeft(area, area.width > 1 ? 1 : 0);
    layout.content = area;
    return layout;
}

}  // namespace mdview::app
