#pragma once

#include "terminal/screen.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"
#include "utils/geometry.hpp"

#include <string>
#include <vector>

namespace mdview::ui {

struct HeaderInfo {
    std::string title;    ///< e.g. "README.md"
    std::string path;     ///< e.g. "docs/README.md"
    std::string details;  ///< right aligned, e.g. "1.4 KB · 320 words"
};

struct StatusHint {
    std::string key;
    std::string label;
};

struct StatusBarInfo {
    std::vector<StatusHint> hints;
    std::string right;
    /// When set, replaces the hints (used for errors and transient notices).
    std::string message;
    bool isError = false;
    /// When set, replaces everything: the incremental search prompt with the
    /// query so far plus the match counter ("3/17").
    std::string searchPrompt;
};

void drawHeaderBar(terminal::Screen& screen, const Rect& area, const HeaderInfo& info,
                   const terminal::Theme& theme);

void drawStatusBar(terminal::Screen& screen, const Rect& area, const StatusBarInfo& info,
                   const terminal::Theme& theme);

/// Centred key-binding reference.  `area` is the whole screen.
void drawHelpOverlay(terminal::Screen& screen, const Rect& area, const terminal::Theme& theme,
                     const terminal::TerminalCapabilities& capabilities);

}  // namespace mdview::ui
