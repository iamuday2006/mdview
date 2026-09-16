#include "ui/tree_view.hpp"

#include "filesystem/file_io.hpp"
#include "utils/unicode.hpp"

#include <algorithm>
#include <string>

namespace mdview::ui {

namespace {

std::string_view ellipsisFor(const terminal::TerminalCapabilities& capabilities) {
    return capabilities.unicode ? "\xE2\x80\xA6" : "..";
}

}  // namespace

void scrollTreeToSelection(TreeViewState& state, std::size_t rowCount, int rows) {
    if (rows <= 0) {
        state.scroll = 0;
        return;
    }
    if (rowCount == 0) {
        state.selected = 0;
        state.scroll = 0;
        return;
    }
    state.selected = std::min(state.selected, rowCount - 1);

    const std::size_t visible = static_cast<std::size_t>(rows);
    if (state.selected < state.scroll) {
        state.scroll = state.selected;
    } else if (state.selected >= state.scroll + visible) {
        state.scroll = state.selected - visible + 1;
    }
    const std::size_t maxScroll = rowCount > visible ? rowCount - visible : 0;
    state.scroll = std::min(state.scroll, maxScroll);
}

void drawTreeView(terminal::Screen& screen, const Rect& area, const filesystem::FileTree& tree,
                  const TreeViewState& state, const terminal::Theme& theme,
                  const terminal::TerminalCapabilities& capabilities, bool focused) {
    if (area.empty()) return;

    screen.pushClip(area);
    screen.fillRect(area, U' ', theme.text);

    const std::vector<filesystem::TreeNode*>& nodes = tree.visibleNodes();
    if (nodes.empty()) {
        screen.drawText(area.x + 1, area.y, "(empty)", theme.muted);
        screen.popClip();
        return;
    }

    const int rows = area.height;
    const int lastColumn = area.right() - 1;

    for (int row = 0; row < rows; ++row) {
        const std::size_t index = state.scroll + static_cast<std::size_t>(row);
        if (index >= nodes.size()) break;

        const filesystem::TreeNode& node = *nodes[index];
        const int y = area.y + row;
        const bool selected = index == state.selected;

        const auto rowStyle = [&]() {
            if (!selected) return theme.text;
            return focused ? theme.treeSelected : theme.treeSelectedMuted;
        }();

        if (selected) {
            screen.fillRect(Rect{area.x, y, area.width, 1}, U' ', rowStyle);
        }

        const std::string indent(static_cast<std::size_t>(std::max(0, node.depth) * 2), ' ');
        std::string glyph;
        if (node.isDirectory) {
            if (capabilities.unicode) {
                glyph = node.expanded ? "\xE2\x96\xBE " : "\xE2\x96\xB8 ";  // ▾ / ▸
            } else {
                glyph = node.expanded ? "- " : "+ ";
            }
        } else {
            glyph = "  ";
        }

        std::string name = node.name;
        if (node.isDirectory) {
            name.push_back('/');
        } else if (node.isSymlink) {
            name.push_back('@');
        }

        const terminal::Style labelStyle = selected
                                               ? rowStyle
                                               : (node.isDirectory ? theme.treeDirectory
                                                                   : (node.isMarkdown ? theme.treeMarkdown
                                                                                      : theme.treeFile));

        std::string sizeText;
        if (!node.isDirectory && node.size > 0) {
            sizeText = filesystem::humanReadableSize(node.size);
        }
        const int sizeWidth = sizeText.empty() ? 0 : uni::displayWidth(sizeText) + 1;
        const int labelMax = std::max(1, area.width - 1 - sizeWidth);

        const std::string label = uni::truncate(indent + glyph + name, labelMax, ellipsisFor(capabilities));
        screen.drawText(area.x, y, label, labelStyle);

        if (!sizeText.empty()) {
            const int sizeX = lastColumn - uni::displayWidth(sizeText);
            if (sizeX > area.x + uni::displayWidth(label)) {
                screen.drawText(sizeX, y, sizeText, selected ? rowStyle : theme.treeGuide);
            }
        }
    }

    screen.popClip();
}

}  // namespace mdview::ui
