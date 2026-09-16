#include "cli/options.hpp"

#include "platform/platform.hpp"

#include <cstdlib>
#include <string_view>

namespace mdview::cli {

namespace {

bool parseWidth(std::string_view text, int& out) {
    if (text.empty()) return false;
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
        if (value > 10000) return false;
    }
    if (value < 20) return false;
    out = value;
    return true;
}

}  // namespace

ParseResult parseOptions(int argc, const char* const argv[]) {
    ParseResult result;
    Options& options = result.options;

    bool sawPath = false;
    bool endOfOptions = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? argv[i] : "";

        if (!endOfOptions && argument == "--") {
            endOfOptions = true;
            continue;
        }

        if (!endOfOptions && argument.size() > 1 && argument.front() == '-') {
            if (argument == "-h" || argument == "--help") {
                options.showHelp = true;
            } else if (argument == "-V" || argument == "--version") {
                options.showVersion = true;
            } else if (argument == "-p" || argument == "--print") {
                options.print = true;
            } else if (argument == "--no-color" || argument == "--no-colour") {
                options.noColor = true;
            } else if (argument == "--ascii") {
                options.ascii = true;
            } else if (argument == "-a" || argument == "--hidden" || argument == "--all") {
                options.showHidden = true;
            } else if (argument == "--no-wrap") {
                options.noWrap = true;
            } else if (argument == "--no-tree") {
                options.showTree = false;
            } else if (argument == "-w" || argument == "--width") {
                if (i + 1 >= argc) {
                    result.ok = false;
                    result.error = std::string(argument) + " needs a value";
                    return result;
                }
                if (!parseWidth(argv[++i], options.width)) {
                    result.ok = false;
                    result.error = "invalid width (expected a number >= 20)";
                    return result;
                }
            } else if (argument.rfind("--width=", 0) == 0) {
                if (!parseWidth(argument.substr(8), options.width)) {
                    result.ok = false;
                    result.error = "invalid width (expected a number >= 20)";
                    return result;
                }
            } else {
                result.ok = false;
                result.error = "unknown option: " + std::string(argument);
                return result;
            }
            continue;
        }

        if (sawPath) {
            result.ok = false;
            result.error = "unexpected extra argument: " + std::string(argument);
            return result;
        }
        options.path = std::string(argument);
        sawPath = true;
    }

    return result;
}

std::string versionText() { return std::string("mdview ") + MDVIEW_VERSION; }

std::string usageText() {
    std::string text;
    text += "mdview " MDVIEW_VERSION " — a terminal Markdown workspace\n";
    text += "Copyright " MDVIEW_YEAR " " MDVIEW_AUTHOR "\n";
    text += MDVIEW_HOMEPAGE "\n";
    text += "\n";
    text += "Usage:\n";
    text += "  mdview [options] [path]\n";
    text += "\n";
    text += "  path  a Markdown file, or a directory to browse (default: .)\n";
    text += "\n";
    text += "Options:\n";
    text += "  -h, --help       show this help and exit\n";
    text += "  -V, --version    show the version and exit\n";
    text += "  -p, --print      render once to stdout instead of starting the TUI\n";
    text += "  -w, --width N    wrap width (default: terminal width, 100 when printing)\n";
    text += "      --no-color   do not emit colours\n";
    text += "      --ascii      use ASCII instead of Unicode box drawing\n";
    text += "      --no-wrap    disable word wrapping (scroll horizontally)\n";
    text += "      --no-tree    hide the file tree pane\n";
    text += "  -a, --hidden     show dotfiles\n";
    text += "\n";
    text += "Keys:\n";
    text += "  q, Ctrl+C   quit            Tab        switch pane\n";
    text += "  Up/Down     move selection  Left/Right expand or scroll\n";
    text += "  PgUp/PgDn   page            Home/End   jump to start/end\n";
    text += "  Enter       open file       r          reload the current file\n";
    text += "  R           refresh tree    w          toggle word wrap\n";
    text += "  .           toggle hidden   [ / ]      previous/next heading\n";
    text += "  / or s      search          n / N      next/previous match\n";
    text += "  ?           toggle help\n";
    text += "\n";
    text += "Platform: ";
    text += platform::name();
    text += "\n";
    text += "\n";
    text += "Author: " MDVIEW_AUTHOR "\n";
    text += "Repository: " MDVIEW_REPOSITORY "\n";
    text += "License: " MDVIEW_LICENSE "\n";
    return text;
}

}  // namespace mdview::cli
