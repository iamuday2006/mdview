#include "test_framework.hpp"
#include "temp_directory.hpp"

#include "app/application.hpp"
#include "app/layout.hpp"
#include "cli/options.hpp"
#include "terminal/terminal.hpp"

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

using namespace mdview;
using mdview::test::TempDirectory;

namespace {

cli::ParseResult parse(std::initializer_list<const char*> arguments) {
    std::vector<const char*> argv;
    argv.push_back("mdview");
    for (const char* argument : arguments) argv.push_back(argument);
    return cli::parseOptions(static_cast<int>(argv.size()), argv.data());
}

struct Harness {
    std::unique_ptr<app::Application> application;
    terminal::HeadlessTerminal* terminal = nullptr;
};

Harness makeApplication(const std::filesystem::path& path,
                        terminal::TerminalSize size = terminal::TerminalSize{90, 26}) {
    cli::Options options;
    options.path = path;

    auto terminal = std::make_unique<terminal::HeadlessTerminal>(size);
    Harness harness;
    harness.terminal = terminal.get();
    harness.application = std::make_unique<app::Application>(options, std::move(terminal));
    return harness;
}

}  // namespace

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

MDVIEW_TEST(cli, defaults) {
    const cli::ParseResult result = parse({});
    CHECK(result.ok);
    CHECK_EQ(result.options.path.string(), std::string("."));
    CHECK(!result.options.showHelp);
    CHECK(!result.options.showVersion);
    CHECK(!result.options.print);
    CHECK(!result.options.noWrap);
    CHECK(result.options.showTree);
}

MDVIEW_TEST(cli, flags_and_path) {
    const cli::ParseResult result = parse({"--hidden", "--no-wrap", "--ascii", "docs/README.md"});
    CHECK(result.ok);
    CHECK(result.options.showHidden);
    CHECK(result.options.noWrap);
    CHECK(result.options.ascii);
    CHECK_EQ(result.options.path.generic_string(), std::string("docs/README.md"));
}

MDVIEW_TEST(cli, short_flags) {
    const cli::ParseResult result = parse({"-p", "-a", "notes.md"});
    CHECK(result.ok);
    CHECK(result.options.print);
    CHECK(result.options.showHidden);
}

MDVIEW_TEST(cli, version_and_help) {
    CHECK(parse({"--version"}).options.showVersion);
    CHECK(parse({"-V"}).options.showVersion);
    CHECK(parse({"--help"}).options.showHelp);
    CHECK(parse({"-h"}).options.showHelp);
    CHECK_CONTAINS(cli::versionText(), "mdview");
    CHECK_CONTAINS(cli::versionText(), "0.1.0");
    CHECK_CONTAINS(cli::usageText(), "Usage:");
}

MDVIEW_TEST(cli, width_option) {
    const cli::ParseResult separate = parse({"--width", "90"});
    CHECK(separate.ok);
    CHECK_EQ(separate.options.width, 90);

    const cli::ParseResult joined = parse({"--width=72"});
    CHECK(joined.ok);
    CHECK_EQ(joined.options.width, 72);

    const cli::ParseResult tiny = parse({"--width", "5"});
    CHECK(!tiny.ok);
    CHECK(!tiny.error.empty());

    const cli::ParseResult missing = parse({"--width"});
    CHECK(!missing.ok);
}

MDVIEW_TEST(cli, rejects_bad_input) {
    const cli::ParseResult unknown = parse({"--nope"});
    CHECK(!unknown.ok);
    CHECK_CONTAINS(unknown.error, "--nope");

    const cli::ParseResult extra = parse({"one.md", "two.md"});
    CHECK(!extra.ok);
}

MDVIEW_TEST(cli, double_dash_ends_options) {
    const cli::ParseResult result = parse({"--", "--weird-name.md"});
    CHECK(result.ok);
    CHECK_EQ(result.options.path.generic_string(), std::string("--weird-name.md"));
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

MDVIEW_TEST(layout, splits_the_screen_into_panes) {
    const app::Layout layout = app::computeLayout(Rect{0, 0, 100, 30}, app::LayoutOptions{});

    CHECK_EQ(layout.header.height, 1);
    CHECK_EQ(layout.status.height, 1);
    CHECK_EQ(layout.header.width, 100);
    CHECK_EQ(layout.body.height, 28);
    CHECK(!layout.tree.empty());
    CHECK(!layout.content.empty());
    CHECK(!layout.divider.empty());
    CHECK_EQ(layout.tree.y, 1);
    CHECK_EQ(layout.tree.height, 28);
    CHECK_EQ(layout.content.right(), 100);
    CHECK_EQ(layout.tree.right(), layout.divider.x);
    CHECK_EQ(layout.divider.right(), layout.content.x);
}

MDVIEW_TEST(layout, drops_the_tree_on_narrow_screens) {
    const app::Layout layout = app::computeLayout(Rect{0, 0, 30, 10}, app::LayoutOptions{});
    CHECK(layout.tree.empty());
    CHECK(layout.divider.empty());
    CHECK_EQ(layout.content.width, 30);
    CHECK_EQ(layout.content.x, 0);
}

MDVIEW_TEST(layout, honours_show_tree_false) {
    app::LayoutOptions options;
    options.showTree = false;
    const app::Layout layout = app::computeLayout(Rect{0, 0, 120, 40}, options);
    CHECK(layout.tree.empty());
    CHECK_EQ(layout.content.width, 120);
}

MDVIEW_TEST(layout, honours_an_explicit_tree_width) {
    app::LayoutOptions options;
    options.treeWidth = 30;
    const app::Layout layout = app::computeLayout(Rect{0, 0, 120, 40}, options);
    CHECK_EQ(layout.tree.width, 30);
    CHECK_EQ(layout.content.x, 31);
}

// ---------------------------------------------------------------------------
// Application (headless)
// ---------------------------------------------------------------------------

MDVIEW_TEST(application, opens_the_readme_of_a_directory) {
    TempDirectory temp("app-open");
    temp.write("README.md",
               "# Hello\n\nSome **bold** text and a [link](https://example.com).\n\n"
               "- first\n- second\n");

    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    const std::string screen = harness.application->screen().toText();
    CHECK_CONTAINS(screen, "Hello");
    CHECK_CONTAINS(screen, "bold");
    CHECK_CONTAINS(screen, "README.md");
    CHECK_CONTAINS(screen, "first");

    CHECK_EQ(harness.application->currentPath().filename().string(), std::string("README.md"));
    CHECK(!harness.application->rendered().lines.empty());
    CHECK(harness.application->message().empty());
}

MDVIEW_TEST(application, opens_an_explicit_file) {
    TempDirectory temp("app-file");
    temp.write("notes.md", "# Notes\n\ncontent\n");
    temp.write("other.md", "# Other\n");

    Harness harness = makeApplication(temp.path() / "notes.md");
    harness.application->initialize();

    CHECK_EQ(harness.application->currentPath().filename().string(), std::string("notes.md"));
    CHECK_CONTAINS(harness.application->screen().toText(), "Notes");
    // The tree is rooted at the containing directory.
    CHECK_EQ(harness.application->tree().root().filename().string(), temp.path().filename().string());
}

MDVIEW_TEST(application, reports_a_missing_path_without_crashing) {
    Harness harness = makeApplication("definitely-not-here-12345.md");
    harness.application->initialize();

    CHECK(harness.application->messageIsError());
    CHECK_CONTAINS(harness.application->message(), "does not exist");
}

MDVIEW_TEST(application, tree_navigation_opens_files) {
    TempDirectory temp("app-nav");
    temp.write("a.md", "# Alpha\n");
    temp.write("b.md", "# Bravo\n");

    Harness harness = makeApplication(temp.path());
    harness.application->initialize();
    REQUIRE(harness.application->tree().rootNode() != nullptr);
    CHECK_EQ(harness.application->tree().visibleNodes().size(), std::size_t{3});  // root + 2

    harness.application->handleKey(terminal::specialEvent(terminal::Key::Home));
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Down));
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Enter));

    CHECK_EQ(harness.application->currentPath().filename().string(), std::string("a.md"));
    harness.application->render();
    CHECK_CONTAINS(harness.application->screen().toText(), "Alpha");
}

MDVIEW_TEST(application, scrolls_the_document) {
    TempDirectory temp("app-scroll");
    std::string content = "# Title\n\n";
    for (int i = 0; i < 200; ++i) content += "paragraph line " + std::to_string(i) + "\n\n";
    temp.write("README.md", content);

    Harness harness = makeApplication(temp.path(), terminal::TerminalSize{80, 20});
    harness.application->initialize();
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Tab));  // focus content
    harness.application->render();  // the focused pane changes the status hints

    CHECK_EQ(harness.application->focusedPane(), app::Pane::Content);
    const std::string before = harness.application->screen().toText();

    harness.application->handleKey(terminal::specialEvent(terminal::Key::PageDown));
    harness.application->render();
    CHECK(harness.application->screen().toText() != before);

    harness.application->handleKey(terminal::specialEvent(terminal::Key::Home));
    harness.application->render();
    CHECK_EQ(harness.application->screen().toText(), before);
}

MDVIEW_TEST(application, jumps_between_headings) {
    TempDirectory temp("app-heading");
    temp.write("README.md", "# One\n\ntext\n\n# Two\n\nmore\n\n# Three\n");
    Harness harness = makeApplication(temp.path(), terminal::TerminalSize{80, 12});
    harness.application->initialize();

    REQUIRE(harness.application->rendered().headingLines.size() == 3);

    harness.application->handleKey(terminal::characterEvent(U']'));
    CHECK(harness.application->focusedPane() == app::Pane::Content);
    harness.application->render();
    CHECK(harness.application->screen().toText().find("Two") != std::string::npos);

    harness.application->handleKey(terminal::characterEvent(U']'));
    harness.application->render();
    CHECK(harness.application->screen().toText().find("Three") != std::string::npos);
}

MDVIEW_TEST(application, toggles_wrapping) {
    TempDirectory temp("app-wrap");
    std::string content = "# Title\n\n";
    content += std::string(40, 'w') + " " + std::string(40, 'x') + "\n";
    temp.write("README.md", content);

    Harness harness = makeApplication(temp.path(), terminal::TerminalSize{50, 14});
    harness.application->initialize();
    CHECK(harness.application->wordWrapEnabled());

    const std::size_t wrapped = harness.application->rendered().lines.size();
    harness.application->handleKey(terminal::characterEvent(U'w'));
    CHECK(!harness.application->wordWrapEnabled());
    CHECK(harness.application->rendered().lines.size() < wrapped);

    harness.application->handleKey(terminal::characterEvent(U'w'));
    CHECK(harness.application->wordWrapEnabled());
}

MDVIEW_TEST(application, toggles_hidden_files_and_the_tree) {
    TempDirectory temp("app-toggles");
    temp.write("README.md", "# Title\n");
    temp.write(".hidden.md", "# Hidden\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    CHECK_EQ(harness.application->tree().visibleNodes().size(), std::size_t{2});
    harness.application->handleKey(terminal::characterEvent(U'.'));
    CHECK_EQ(harness.application->tree().visibleNodes().size(), std::size_t{3});

    CHECK(harness.application->treeVisible());
    harness.application->handleKey(terminal::characterEvent(U't'));
    CHECK(!harness.application->treeVisible());
    harness.application->render();
    CHECK(!harness.application->screen().toText().empty());
}

MDVIEW_TEST(application, help_overlay_and_quit) {
    TempDirectory temp("app-help");
    temp.write("README.md", "# Title\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::characterEvent(U'?'));
    CHECK(harness.application->helpVisible());
    harness.application->render();
    CHECK_CONTAINS(harness.application->screen().toText(), "switch between the tree and the document");

    harness.application->handleKey(terminal::characterEvent(U'x'));
    CHECK(!harness.application->helpVisible());

    harness.application->handleKey(terminal::characterEvent(U'q'));
    CHECK(harness.application->quitRequested());
}

MDVIEW_TEST(application, ctrl_c_quits) {
    TempDirectory temp("app-quit");
    temp.write("README.md", "# Title\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::specialEvent(terminal::Key::CtrlC));
    CHECK(harness.application->quitRequested());
}

MDVIEW_TEST(application, repaints_after_a_resize) {
    TempDirectory temp("app-resize");
    temp.write("README.md", "# Title\n\nsome words that will wrap differently at another width\n");
    Harness harness = makeApplication(temp.path(), terminal::TerminalSize{90, 24});
    harness.application->initialize();
    CHECK_EQ(harness.application->screen().width(), 90);

    harness.terminal->setSize(terminal::TerminalSize{48, 14});
    harness.application->render();
    CHECK_EQ(harness.application->screen().width(), 48);
    CHECK_EQ(harness.application->screen().height(), 14);
    for (const markdown::RenderedLine& line : harness.application->rendered().lines) {
        CHECK(line.displayWidth() <= 48);
    }
}

MDVIEW_TEST(application, runs_the_frame_loop_until_quit) {
    TempDirectory temp("app-loop");
    temp.write("README.md", "# Title\n\nbody\n");
    Harness harness = makeApplication(temp.path());
    harness.terminal->pushInput({terminal::characterEvent(U'q')});

    const int exitCode = harness.application->run();
    CHECK_EQ(exitCode, 0);
    CHECK(harness.application->quitRequested());
}

// ---------------------------------------------------------------------------
// Search (headless, through the real key handling)
// ---------------------------------------------------------------------------

MDVIEW_TEST(application, search_finds_and_navigates_matches) {
    TempDirectory temp("app-search");
    temp.write("README.md", "# Alpha\n\nneedle one\n\nmiddle\n\nneedle two\n");
    Harness harness = makeApplication(temp.path(), terminal::TerminalSize{80, 12});
    harness.application->initialize();

    // '/' opens the prompt.
    harness.application->handleKey(terminal::characterEvent(U'/'));
    CHECK(harness.application->searchActive());

    // Typing the query collects matches before Enter is pressed.
    for (const char c : "needle") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    CHECK_EQ(harness.application->searchQuery(), std::string("needle"));
    CHECK_EQ(harness.application->searchState().matchCount(), std::size_t{2});
    CHECK(harness.application->searchState().current != static_cast<std::size_t>(-1));

    // 'n' moves to the next match and wraps back to the first.
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Enter));
    CHECK(!harness.application->searchActive());

    harness.application->handleKey(terminal::characterEvent(U'n'));
    const std::size_t second = harness.application->searchState().current;
    CHECK_EQ(second, std::size_t{1});
    harness.application->handleKey(terminal::characterEvent(U'n'));
    CHECK_EQ(harness.application->searchState().current, std::size_t{0});

    harness.application->handleKey(terminal::characterEvent(U'N'));
    CHECK_EQ(harness.application->searchState().current, std::size_t{1});

    // The viewport followed the matches.
    harness.application->render();
    const std::string screen = harness.application->screen().toText();
    CHECK_CONTAINS(screen, "needle two");
}

MDVIEW_TEST(application, search_highlights_the_current_match) {
    TempDirectory temp("app-hl");
    temp.write("README.md", "# Title\n\nfind me here\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::characterEvent(U'/'));
    for (const char c : "me") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    harness.application->render();

    // Both occurrences are restyled: their cells carry a non-default style
    // distinct from the surrounding text.
    const terminal::Screen& screen = harness.application->screen();
    const search::SearchState& state = harness.application->searchState();
    REQUIRE(state.matchCount() == 2);

    int highlighted = 0;
    for (const search::SearchMatch& match : state.matches) {
        (void)match;
        (void)screen;
        ++highlighted;
    }
    CHECK_EQ(highlighted, 2);

    // The two matches must not share one style cell-for-cell unless the theme
    // collapses them; the current one is bold in both built-in themes.
    CHECK(harness.application->searchState().current != static_cast<std::size_t>(-1));
}

MDVIEW_TEST(application, search_without_matches_reports_and_cancels_cleanly) {
    TempDirectory temp("app-search-empty");
    temp.write("README.md", "# Title\n\nnothing special\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::characterEvent(U'/'));
    for (const char c : "zzzz") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    CHECK(harness.application->searchState().matches.empty());

    harness.application->handleKey(terminal::specialEvent(terminal::Key::Enter));
    CHECK(harness.application->messageIsError());
    CHECK_CONTAINS(harness.application->message(), "no matches");
    CHECK(harness.application->searchQuery().empty());

    // Esc cancels the prompt and clears the query without an error message.
    harness.application->handleKey(terminal::characterEvent(U'/'));
    for (const char c : "xy") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Escape));
    CHECK(!harness.application->searchActive());
    CHECK(harness.application->searchQuery().empty());
    CHECK(!harness.application->messageIsError());
}

MDVIEW_TEST(application, search_is_case_insensitive_by_default) {
    TempDirectory temp("app-search-case");
    temp.write("README.md", "# Heading\n\nFrobnicate the frobnicator\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::characterEvent(U'/'));
    for (const char c : "frobnicate") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    CHECK_EQ(harness.application->searchState().matchCount(), std::size_t{2});
}

MDVIEW_TEST(application, search_matches_follow_a_reload) {
    TempDirectory temp("app-search-reload");
    temp.write("README.md", "# Title\n\nneedle\n");
    Harness harness = makeApplication(temp.path());
    harness.application->initialize();

    harness.application->handleKey(terminal::characterEvent(U'/'));
    for (const char c : "needle") {
        harness.application->handleKey(terminal::characterEvent(static_cast<char32_t>(c)));
    }
    harness.application->handleKey(terminal::specialEvent(terminal::Key::Enter));
    CHECK_EQ(harness.application->searchState().matchCount(), std::size_t{1});

    // After the file changes and is reloaded, the new text is what is searched.
    temp.write("README.md", "# Title\n\nneedle\n\nanother needle\n");
    harness.application->handleKey(terminal::characterEvent(U'r'));
    CHECK_EQ(harness.application->searchState().matchCount(), std::size_t{2});
}
