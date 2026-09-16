#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Command line parsing, kept separate from the application so it can be tested
// without a terminal and so main() stays a handful of lines.

namespace mdview::cli {

#ifndef MDVIEW_VERSION
#define MDVIEW_VERSION "0.0.0"
#endif

#ifndef MDVIEW_AUTHOR
#define MDVIEW_AUTHOR "unknown"
#endif

#ifndef MDVIEW_HOMEPAGE
#define MDVIEW_HOMEPAGE ""
#endif

#ifndef MDVIEW_REPOSITORY
#define MDVIEW_REPOSITORY ""
#endif

#ifndef MDVIEW_COPYRIGHT
#define MDVIEW_COPYRIGHT ""
#endif

#ifndef MDVIEW_LICENSE
#define MDVIEW_LICENSE ""
#endif

#ifndef MDVIEW_YEAR
#define MDVIEW_YEAR ""
#endif

struct Options {
    /// File or directory to open.  Defaults to the current directory.
    std::filesystem::path path = ".";

    bool showHelp = false;
    bool showVersion = false;

    /// Render the document to stdout once and exit (no TUI).  Useful for
    /// piping into a pager and for scripted verification.
    bool print = false;

    bool noColor = false;
    bool ascii = false;
    bool showHidden = false;
    bool noWrap = false;
    bool showTree = true;

    /// 0 means "detect from the terminal".
    int width = 0;
};

struct ParseResult {
    Options options;
    bool ok = true;
    std::string error;
};

ParseResult parseOptions(int argc, const char* const argv[]);

std::string usageText();
std::string versionText();

}  // namespace mdview::cli
