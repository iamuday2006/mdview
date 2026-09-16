#include "app/application.hpp"
#include "cli/options.hpp"
#include "filesystem/file_io.hpp"
#include "markdown/parser.hpp"
#include "markdown/renderer.hpp"
#include "platform/platform.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"
#include "utils/env.hpp"
#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace stdfs = std::filesystem;

/// Prefers README.md, then any other README, then index.md.
int documentRank(const std::string& name) {
    const std::string lower = mdview::str::toLowerAscii(name);
    if (lower == "readme.md") return 0;
    if (mdview::str::startsWith(lower, "readme")) return 1;
    if (lower == "index.md") return 2;
    return 3;
}

stdfs::path pickDocumentInDirectory(const stdfs::path& directory) {
    std::error_code error;
    stdfs::path best;
    int bestRank = 4;

    stdfs::directory_iterator iterator(directory, stdfs::directory_options::skip_permission_denied, error);
    if (error) return {};

    for (const stdfs::directory_entry& entry : iterator) {
        std::error_code entryError;
        if (!entry.is_regular_file(entryError)) continue;
        if (!mdview::filesystem::isMarkdownPath(entry.path())) continue;
        const int rank = documentRank(entry.path().filename().string());
        if (rank < bestRank) {
            bestRank = rank;
            best = entry.path();
        }
    }
    return best;
}

void printDirectoryListing(const stdfs::path& directory) {
    std::error_code error;
    stdfs::directory_iterator iterator(directory, stdfs::directory_options::skip_permission_denied, error);
    if (error) {
        std::fprintf(stderr, "mdview: cannot list '%s'\n", directory.string().c_str());
        return;
    }

    std::printf("%s\n", directory.string().c_str());
    std::vector<std::string> directories;
    std::vector<std::string> files;
    for (const stdfs::directory_entry& entry : iterator) {
        std::error_code entryError;
        const std::string name = entry.path().filename().string();
        if (entry.is_directory(entryError)) {
            directories.push_back(name + "/");
        } else {
            files.push_back(name);
        }
    }
    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end());
    for (const std::string& name : directories) std::printf("  %s\n", name.c_str());
    for (const std::string& name : files) std::printf("  %s\n", name.c_str());
}

/// Renders one document to stdout.  This is the same parser and renderer the
/// TUI uses, just without the interactive shell, which makes it useful both for
/// piping into a pager and for verifying the renderer in a script.
int printDocument(const mdview::cli::Options& options) {
    stdfs::path target = options.path;
    std::error_code error;
    const stdfs::file_status status = stdfs::status(target, error);
    if (error || !stdfs::exists(status)) {
        std::fprintf(stderr, "mdview: no such file or directory: '%s'\n", target.string().c_str());
        return 1;
    }

    if (stdfs::is_directory(status)) {
        const stdfs::path candidate = pickDocumentInDirectory(target);
        if (candidate.empty()) {
            printDirectoryListing(target);
            return 0;
        }
        target = candidate;
    }

    const mdview::filesystem::ReadResult file = mdview::filesystem::readTextFile(target);
    if (!file.ok) {
        std::fprintf(stderr, "mdview: cannot read '%s': %s\n", target.string().c_str(),
                     file.error.c_str());
        return 1;
    }

    const bool interactive = mdview::platform::stdoutIsTerminal();
    const bool colored = interactive && !options.noColor;

    mdview::terminal::Theme theme = colored
                                        ? mdview::terminal::Theme::colored(mdview::terminal::ColorMode::Ansi256)
                                        : mdview::terminal::Theme::plain();

    const int width = options.width > 0
                          ? options.width
                          : (interactive ? std::max(40, mdview::env::getInt("COLUMNS", 100)) : 100);

    mdview::markdown::RenderOptions renderOptions;
    renderOptions.width = width;
    renderOptions.theme = &theme;
    renderOptions.syntaxHighlight = colored;
    renderOptions.unicode = !options.ascii;
    renderOptions.wrap = !options.noWrap;

    const mdview::markdown::Document document =
        mdview::markdown::parseMarkdown(file.text);
    const mdview::markdown::RenderedDocument rendered =
        mdview::markdown::renderMarkdown(document, renderOptions);

    const mdview::terminal::ColorMode mode =
        colored ? mdview::terminal::ColorMode::Ansi256 : mdview::terminal::ColorMode::None;

    std::string output;
    output.reserve(64 * 1024);
    for (const mdview::markdown::RenderedLine& line : rendered.lines) {
        std::string row;
        mdview::terminal::Style active;
        bool styleKnown = false;
        for (const mdview::markdown::Span& span : line.spans) {
            if (mode != mdview::terminal::ColorMode::None &&
                (!styleKnown || !(span.style == active))) {
                row += mdview::terminal::sgrFor(span.style, mode);
                active = span.style;
                styleKnown = true;
            }
            row += span.text;
        }
        std::size_t end = row.size();
        while (end > 0 && row[end - 1] == ' ') --end;
        row.resize(end);
        if (mode != mdview::terminal::ColorMode::None) row += "\x1b[0m";
        output += row;
        output.push_back('\n');
    }

    if (!output.empty()) {
        std::fwrite(output.data(), 1, output.size(), stdout);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const mdview::cli::ParseResult parsed = mdview::cli::parseOptions(argc, argv);
    if (!parsed.ok) {
        std::fprintf(stderr, "mdview: %s\n\n%s", parsed.error.c_str(), mdview::cli::usageText().c_str());
        return 2;
    }

    const mdview::cli::Options& options = parsed.options;

    if (options.showHelp) {
        const std::string text = mdview::cli::usageText();
        std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }
    if (options.showVersion) {
        const std::string text = mdview::cli::versionText() + "\n";
        std::fwrite(text.data(), 1, text.size(), stdout);
        return 0;
    }
    if (options.print) {
        return printDocument(options);
    }

    // The TUI needs a real terminal on both ends.  Saying so plainly is far
    // friendlier than half-drawing a screen into a pipe.
    if (!mdview::platform::stdoutIsTerminal() || !mdview::platform::stdinIsTerminal()) {
        std::fprintf(stderr,
                     "mdview: no interactive terminal detected (stdout or stdin is redirected).\n"
                     "        Render once to stdout instead:\n"
                     "            mdview --print %s\n",
                     options.path.string().c_str());
        return 1;
    }

    std::unique_ptr<mdview::terminal::Terminal> terminal = mdview::terminal::createTerminal();
    if (!terminal) {
        std::fprintf(stderr, "mdview: this platform has no terminal implementation\n");
        return 1;
    }

    mdview::app::Application application(options, std::move(terminal));
    return application.run();
}
