#pragma once

#include "app/layout.hpp"
#include "cli/options.hpp"
#include "filesystem/file_tree.hpp"
#include "filesystem/file_watcher.hpp"
#include "markdown/document.hpp"
#include "markdown/renderer.hpp"
#include "search/search.hpp"
#include "terminal/screen.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"
#include "ui/markdown_view.hpp"
#include "ui/tree_view.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace mdview::app {

enum class Pane { Tree, Content };

/// Owns all application state and drives the frame loop.
///
/// The pieces are deliberately public so the UI can be exercised without a real
/// terminal: a test can construct an Application on a HeadlessTerminal, feed
/// KeyEvents into handleKey() and assert on screen().
class Application {
public:
    Application(cli::Options options, std::unique_ptr<terminal::Terminal> terminal);
    ~Application();

    /// Initializes, then runs frames until the user quits.
    int run();

    void initialize();
    void handleKey(const terminal::KeyEvent& event);
    void render();
    void shutdown();

    // --- introspection for tests and for main() ---------------------------
    const terminal::Screen& screen() const { return screen_; }
    const cli::Options& options() const { return options_; }
    bool quitRequested() const { return quit_; }
    Pane focusedPane() const { return focus_; }
    const std::filesystem::path& currentPath() const { return currentPath_; }
    const markdown::Document& document() const { return document_; }
    const markdown::RenderedDocument& rendered() const { return rendered_; }
    const filesystem::FileTree& tree() const { return tree_; }
    const std::string& message() const { return message_; }
    bool messageIsError() const { return messageIsError_; }
    bool wordWrapEnabled() const { return wrap_; }
    bool treeVisible() const { return showTree_; }
    bool helpVisible() const { return showHelp_; }
    /// True while the incremental search prompt is open.
    bool searchActive() const { return searchMode_; }
    const search::SearchState& searchState() const { return search_; }
    const std::string& searchQuery() const { return search_.query.text; }

private:
    LayoutOptions layoutOptions() const;
    void applyTheme();
    void openPath(const std::filesystem::path& path);
    void openFile(const std::filesystem::path& path, bool keepScroll = false);
    void reloadCurrentFile();
    void renderDocument();
    void selectPathInTree(const std::filesystem::path& path);
    void openBestDocumentInRoot();
    void syncWatcher();
    void setMessage(std::string text, bool isError);

    void moveSelection(int delta);
    void moveByPage(int direction);
    void jumpToStart();
    void jumpToEnd();
    void activateSelection();
    void expandSelection();
    void collapseSelection();
    void scrollContent(int deltaLines);
    void scrollContentHorizontal(int deltaColumns);
    void jumpToHeading(int direction);
    void toggleWrap();
    void toggleHidden();
    void toggleTree();
    void cycleFocus();

    // --- search ------------------------------------------------------------
    void beginSearch();
    void handleSearchKey(const terminal::KeyEvent& event);
    void endSearch(bool commit);
    void refreshSearch();
    void stepSearch(int direction);
    void scrollToCurrentMatch();
    void cancelSearch();

    int contentWidth() const;
    std::size_t maximumContentScroll() const;
    std::string headerDetails() const;
    std::string emptyMessage() const;
    std::string rightStatus() const;

    cli::Options options_;
    std::unique_ptr<terminal::Terminal> terminal_;

    terminal::Screen screen_;
    terminal::Theme theme_;
    terminal::TerminalCapabilities capabilities_;

    filesystem::FileTree tree_;
    std::unique_ptr<filesystem::FileWatcher> watcher_;
    std::filesystem::path watchedPath_;

    markdown::Document document_;
    markdown::RenderedDocument rendered_;
    std::filesystem::path currentPath_;
    std::string currentTitle_;
    std::uintmax_t currentSize_ = 0;
    std::size_t wordCount_ = 0;

    std::string message_;
    bool messageIsError_ = false;

    ui::TreeViewState treeState_;
    ui::MarkdownViewState contentState_;
    Pane focus_ = Pane::Tree;

    Layout layout_;
    bool wrap_ = true;
    bool showTree_ = true;
    bool showHelp_ = false;
    bool quit_ = false;
    bool initialized_ = false;
    int renderedWidth_ = 0;
    terminal::TerminalSize lastSize_{};

    bool searchMode_ = false;
    search::SearchState search_;
    std::string searchStatus_;  ///< e.g. "3/17", "no matches"
};

}  // namespace mdview::app
