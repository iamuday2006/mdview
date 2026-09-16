#include "ui/chrome.hpp"

#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <algorithm>
#include <array>

namespace mdview::ui {

void drawHeaderBar(terminal::Screen& screen, const Rect& area, const HeaderInfo& info,
                   const terminal::Theme& theme) {
    if (area.empty()) return;

    screen.pushClip(area);
    screen.fillRect(area, U' ', theme.headerBar);

    int x = area.x + 1;
    const int limit = area.right() - 1;

    if (!info.title.empty()) {
        const std::string title = uni::truncate(info.title, std::max(1, limit - x), "\xE2\x80\xA6");
        screen.drawText(x, area.y, title, theme.headerTitle);
        x += uni::displayWidth(title);
    }
    if (!info.path.empty() && x + 2 < limit) {
        const std::string path = uni::truncate(info.path, std::max(1, limit - x - 2), "\xE2\x80\xA6");
        screen.drawText(x + 2, area.y, path, theme.headerPath);
    }
    if (!info.details.empty() && area.width > 12) {
        const int detailsWidth = uni::displayWidth(info.details);
        const int detailsX = limit - detailsWidth;
        if (detailsX > x + 2) {
            screen.drawText(detailsX, area.y, info.details, theme.headerPath);
        }
    }
    screen.popClip();
}

void drawStatusBar(terminal::Screen& screen, const Rect& area, const StatusBarInfo& info,
                   const terminal::Theme& theme) {
    if (area.empty()) return;

    screen.pushClip(area);
    screen.fillRect(area, U' ', theme.statusBar);

    int x = area.x + 1;
    const int limit = area.right() - 1;

    if (!info.searchPrompt.empty()) {
        // The search prompt owns the whole bar; the match count rides on the
        // right so a long query cannot push it out of view.
        const std::string text =
            uni::truncate(info.searchPrompt, std::max(1, limit - x), "\xE2\x80\xA6");
        screen.drawText(x, area.y, text, theme.accent);
        screen.drawText(area.x, area.y, " ", theme.statusBar);
        if (!info.right.empty() && area.width > 10) {
            const int rightWidth = uni::displayWidth(info.right);
            const int rightX = limit - rightWidth;
            if (rightX > x) {
                screen.drawText(rightX, area.y, info.right, theme.statusText);
            }
        }
        screen.popClip();
        return;
    }

    if (!info.message.empty()) {
        const std::string text =
            uni::truncate(info.message, std::max(1, limit - x), "\xE2\x80\xA6");
        screen.drawText(x, area.y, text, info.isError ? theme.error : theme.warning);
    } else {
        for (const StatusHint& hint : info.hints) {
            if (hint.key.empty()) continue;
            const int width = uni::displayWidth(hint.key) + 1 + uni::displayWidth(hint.label) + 3;
            if (x + width > limit) break;
            screen.drawText(x, area.y, hint.key, theme.statusKey);
            x += uni::displayWidth(hint.key) + 1;
            screen.drawText(x, area.y, hint.label, theme.statusText);
            x += uni::displayWidth(hint.label) + 3;
        }
    }

    if (!info.right.empty() && area.width > 10) {
        const int rightWidth = uni::displayWidth(info.right);
        const int rightX = limit - rightWidth;
        if (rightX > x) {
            screen.drawText(rightX, area.y, info.right, theme.statusText);
        }
    }
    screen.popClip();
}

void drawHelpOverlay(terminal::Screen& screen, const Rect& area, const terminal::Theme& theme,
                     const terminal::TerminalCapabilities& capabilities) {
    struct Entry {
        const char* key;
        const char* description;
    };
    static constexpr std::array<Entry, 23> kEntries{{
        {"Tab", "switch between the tree and the document"},
        {"Up / Down", "move the selection, or scroll the document"},
        {"Left / Right", "collapse/expand a directory, or scroll horizontally"},
        {"PgUp / PgDn", "page through the document"},
        {"Home / End", "jump to the start or the end"},
        {"Enter", "open the selected file, or toggle a directory"},
        {"/ or s", "search in the document"},
        {"n / N", "next or previous search match"},
        {"[ / ]", "jump to the previous or next heading"},
        {"r", "reload the current file from disk"},
        {"R", "refresh the file tree"},
        {"w", "toggle word wrapping"},
        {".", "show or hide dotfiles"},
        {"t", "show or hide the file tree"},
        {"g / G", "go to the first or last line"},
        {"?", "close this help"},
        {"q", "quit"},
        {"Ctrl+C", "quit"},
        {"Ctrl+L", "repaint the screen"},
        {"Ctrl+U", "scroll up by half a page / clear the search query"},
        {"Enter", "accept the search query"},
        {"Esc", "cancel search, close this help"},
    }};

    // The panel is centred and sized to its content, but never larger than the
    // available screen.
    int widest = 0;
    for (const Entry& entry : kEntries) {
        widest = std::max(widest, static_cast<int>(std::string_view(entry.key).size()) + 3 +
                                      static_cast<int>(std::string_view(entry.description).size()));
    }
    const int panelWidth = std::clamp(widest + 6, 24, std::max(24, area.width - 4));
    const int panelHeight = std::clamp(static_cast<int>(kEntries.size()) + 4, 6, std::max(6, area.height - 2));
    const Rect panel{area.x + std::max(0, (area.width - panelWidth) / 2),
                     area.y + std::max(0, (area.height - panelHeight) / 2), panelWidth, panelHeight};

    // Every style used inside the panel carries the same background, so the
    // overlay reads as a modal instead of as text floating over the document.
    screen.pushClip(panel);
    screen.fillRect(panel, U' ', theme.helpText);
    screen.drawFrame(panel, capabilities.unicode ? terminal::BorderStyle::Rounded : terminal::BorderStyle::Ascii,
                     theme.borderFocused, "mdview - keys", theme.helpTitle);

    const Rect inner = panel.inset(1);
    int row = inner.y;
    for (const Entry& entry : kEntries) {
        if (row >= inner.bottom()) break;
        screen.drawText(inner.x + 2, row, entry.key, theme.helpKey);
        const int labelX = inner.x + 2 + 14;
        if (labelX < inner.right() - 1) {
            screen.drawText(labelX, row, uni::truncate(entry.description, inner.right() - 1 - labelX, "\xE2\x80\xA6"),
                            theme.helpText);
        }
        ++row;
    }

    if (inner.bottom() - inner.y > static_cast<int>(kEntries.size())) {
        screen.drawText(inner.x + 2, inner.bottom() - 2, "MDVIEW", theme.accent);
        const std::string hint = "any key to close";
        const int hintWidth = uni::displayWidth(hint);
        if (inner.right() - 2 - hintWidth > inner.x) {
            screen.drawText(inner.right() - 2 - hintWidth, inner.bottom() - 2, hint, theme.muted);
        }
    }
    screen.popClip();
}

}  // namespace mdview::ui
