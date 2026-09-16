#include "app/application.hpp"

#include "filesystem/file_io.hpp"
#include "markdown/parser.hpp"
#include "platform/platform.hpp"
#include "ui/chrome.hpp"
#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <system_error>
#include <utility>

namespace mdview::app {

namespace {

namespace stdfs = std::filesystem;

std::size_t countWords(std::string_view text) {
    std::size_t words = 0;
    bool inWord = false;
    for (const char c : text) {
        const bool isSpace = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (isSpace) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++words;
        }
    }
    return words;
}

/// Case-insensitive compare used to prefer README.md when opening a directory.
int readmeRank(const std::string& name) {
    const std::string lower = str::toLowerAscii(name);
    if (lower == "readme.md") return 0;
    if (str::startsWith(lower, "readme")) return 1;
    if (lower == "index.md") return 2;
    return 3;
}

}  // namespace

Application::Application(cli::Options options, std::unique_ptr<terminal::Terminal> terminal)
    : options_(std::move(options)), terminal_(std::move(terminal)) {
    wrap_ = !options_.noWrap;
    showTree_ = options_.showTree;
}

Application::~Application() = default;

LayoutOptions Application::layoutOptions() const {
    LayoutOptions layoutOptions;
    layoutOptions.showTree = showTree_;
    layoutOptions.treeWidth = 0;
    return layoutOptions;
}

void Application::applyTheme() {
    if (capabilities_.colorMode == terminal::ColorMode::None || options_.noColor) {
        capabilities_.colorMode = terminal::ColorMode::None;
        theme_ = terminal::Theme::plain();
    } else {
        theme_ = terminal::Theme::colored(capabilities_.colorMode);
    }
    if (options_.ascii) capabilities_.unicode = false;
}

void Application::initialize() {
    if (initialized_) return;
    initialized_ = true;

    terminal_->initialize();
    capabilities_ = terminal_->capabilities();
    applyTheme();

    lastSize_ = terminal_->size();
    screen_.resize(lastSize_.width, lastSize_.height);

    tree_.setShowHidden(options_.showHidden);
    openPath(options_.path);

    if (capabilities_.ansi && !currentTitle_.empty()) {
        terminal_->write("\x1b]0;mdview \xE2\x80\x94 " + currentTitle_ + "\x07");
    }

    screen_.invalidate();
    render();
}

void Application::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    terminal_->shutdown();
}

// ---------------------------------------------------------------------------
// Opening documents
// ---------------------------------------------------------------------------

void Application::openPath(const stdfs::path& path) {
    std::error_code error;
    stdfs::path target = path.empty() ? stdfs::path(".") : path;
    const stdfs::path resolved = stdfs::weakly_canonical(target, error);
    if (!error && !resolved.empty()) target = resolved;

    const stdfs::file_status status = stdfs::status(target, error);
    if (error || !stdfs::exists(status)) {
        setMessage("path does not exist: " + target.string(), true);
        stdfs::path fallback = stdfs::current_path(error);
        if (error) fallback = stdfs::path(".");
        tree_.setRoot(fallback);
        renderDocument();
        return;
    }

    if (stdfs::is_directory(status)) {
        tree_.setRoot(target);
        treeState_.selected = 0;
        treeState_.scroll = 0;
        focus_ = showTree_ ? Pane::Tree : Pane::Content;
        openBestDocumentInRoot();
        return;
    }

    stdfs::path parent = target.parent_path();
    if (parent.empty()) parent = stdfs::current_path(error);
    if (error || parent.empty()) parent = stdfs::path(".");
    tree_.setRoot(parent);
    treeState_.selected = 0;
    treeState_.scroll = 0;
    focus_ = Pane::Content;
    openFile(target);
}

void Application::openBestDocumentInRoot() {
    const filesystem::TreeNode* root = tree_.rootNode();
    if (root == nullptr) {
        renderDocument();
        return;
    }

    const filesystem::TreeNode* best = nullptr;
    int bestRank = 4;
    for (const auto& child : root->children) {
        if (child->isDirectory || !child->isMarkdown) continue;
        const int rank = readmeRank(child->name);
        if (rank < bestRank) {
            bestRank = rank;
            best = child.get();
        }
    }

    if (best == nullptr) {
        renderDocument();
        return;
    }
    openFile(best->path);
}

void Application::openFile(const stdfs::path& path, bool keepScroll) {
    const filesystem::ReadResult result = filesystem::readTextFile(path);
    if (!result.ok) {
        setMessage("cannot read " + path.filename().string() + ": " + result.error, true);
        renderDocument();
        return;
    }

    document_ = markdown::parseMarkdown(result.text);
    currentPath_ = path;
    currentTitle_ = path.filename().string();
    currentSize_ = result.size;
    wordCount_ = countWords(result.text);

    if (!keepScroll) {
        contentState_.scroll = 0;
        contentState_.horizontalScroll = 0;
    }
    renderDocument();
    syncWatcher();
    selectPathInTree(path);
    setMessage({}, false);
}

void Application::reloadCurrentFile() {
    if (currentPath_.empty()) {
        tree_.refresh();
        return;
    }
    const std::size_t keepScroll = contentState_.scroll;
    openFile(currentPath_, /*keepScroll=*/true);
    contentState_.scroll = keepScroll;
    setMessage("reloaded " + currentTitle_, false);
}

/// Rebuilds match positions after the document re-rendered (reload, wrap
/// toggle, resize).  Matches are indices into rendered lines, so they cannot
/// survive a re-render; re-running the query is exact and simpler than trying
/// to remap old positions onto new lines.

void Application::renderDocument() {
    markdown::RenderOptions renderOptions;
    renderOptions.width = std::max(20, contentWidth());
    renderOptions.theme = &theme_;
    renderOptions.syntaxHighlight = true;
    renderOptions.unicode = capabilities_.unicode;
    renderOptions.wrap = wrap_;

    rendered_ = markdown::renderMarkdown(document_, renderOptions);
    renderedWidth_ = renderOptions.width;

    // Rendered line indices just changed, so any collected matches are stale.
    // Re-running the query is cheaper and simpler than trying to remap it.
    if (search_.active()) refreshSearch();

    const std::size_t maximum = maximumContentScroll();
    contentState_.scroll = std::min(contentState_.scroll, maximum);
}

void Application::selectPathInTree(const stdfs::path& path) {
    const std::size_t index = tree_.indexOf(path);
    if (index != static_cast<std::size_t>(-1)) treeState_.selected = index;
}

void Application::syncWatcher() {
    if (currentPath_.empty()) return;
    if (!watcher_) watcher_ = filesystem::createFileWatcher();
    if (watchedPath_ == currentPath_) return;
    if (!watchedPath_.empty()) watcher_->unwatch(watchedPath_);
    watcher_->watch(currentPath_);
    watchedPath_ = currentPath_;
}

void Application::setMessage(std::string text, bool isError) {
    message_ = std::move(text);
    messageIsError_ = isError;
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

int Application::contentWidth() const {
    const Layout layout = computeLayout(screen_.bounds(), layoutOptions());
    // One column is reserved for the scrollbar.
    return std::max(20, layout.content.width - 1);
}

std::size_t Application::maximumContentScroll() const {
    const std::size_t rows =
        layout_.content.height > 0 ? static_cast<std::size_t>(layout_.content.height) : 0;
    const std::size_t total = rendered_.lines.size();
    return total > rows ? total - rows : 0;
}

std::string Application::headerDetails() const {
    if (currentPath_.empty()) return {};
    std::string details = filesystem::humanReadableSize(currentSize_);
    details += "  \xC2\xB7  ";
    details += std::to_string(wordCount_);
    details += " words  \xC2\xB7  ";
    details += std::to_string(rendered_.lines.size());
    details += " lines";
    return details;
}

std::string Application::emptyMessage() const {
    if (!message_.empty()) return message_;
    if (currentPath_.empty()) {
        return "Select a Markdown file in the tree and press Enter.";
    }
    return {};
}

std::string Application::rightStatus() const {
    if (currentPath_.empty()) return {};
    const std::size_t rows =
        layout_.content.height > 0 ? static_cast<std::size_t>(layout_.content.height) : 1;
    const std::size_t maximum = maximumContentScroll();
    const int percent =
        maximum == 0 ? 100 : static_cast<int>(std::lround(100.0 * static_cast<double>(contentState_.scroll) /
                                                          static_cast<double>(maximum)));
    std::string text = std::to_string(contentState_.scroll + std::min<std::size_t>(rows, rendered_.lines.size()));
    text += "/";
    text += std::to_string(rendered_.lines.size());
    text += "  ";
    text += std::to_string(percent);
    text += "%";
    if (!wrap_) text += "  [no wrap]";
    return text;
}

// ---------------------------------------------------------------------------
// Key handling
// ---------------------------------------------------------------------------

void Application::cycleFocus() {
    focus_ = focus_ == Pane::Tree ? Pane::Content : Pane::Tree;
    if (layout_.tree.empty() && focus_ == Pane::Tree) focus_ = Pane::Content;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void Application::beginSearch() {
    searchMode_ = true;
    search_.current = static_cast<std::size_t>(-1);
    searchStatus_.clear();
}

/// Matches are collected in one pass over the rendered lines.  Searching the
/// rendered text rather than the source keeps the model honest: what the user
/// sees is what is searched, including wrapped lines, and every match comes
/// with the display columns the highlighter needs.
void Application::refreshSearch() {
    search_.matches.clear();
    search_.current = static_cast<std::size_t>(-1);
    searchStatus_.clear();
    if (search_.query.empty()) return;

    const std::string& needle = search_.query.text;
    const bool caseSensitive = search_.query.caseSensitive;
    for (std::size_t line = 0; line < rendered_.lines.size(); ++line) {
        std::string text;
        text.reserve(64);
        for (const markdown::Span& span : rendered_.lines[line].spans) text += span.text;
        if (search::findMatchesInLine(text, needle, caseSensitive).empty()) continue;

        // Column offsets only need computing when the line actually matched.
        const std::vector<int> offsets = search::columnOffsets(text);
        for (const std::size_t at : search::findMatchesInLine(text, needle, caseSensitive)) {
            const std::size_t end = at + needle.size();
            const int endColumn = end < offsets.size() ? offsets[end] : offsets.back();
            search_.matches.push_back(search::SearchMatch{line, offsets[at], endColumn});
        }
    }

    if (search_.matches.empty()) {
        searchStatus_ = "no matches";
    }
}

/// Formats the "3/17" counter shown while searching and in the post-search
/// status message.
std::string matchPosition(const search::SearchState& search) {
    if (search.matches.empty()) return "0/0";
    const std::size_t ordinal =
        search.current < search.matches.size() ? search.current + 1 : 1;
    return std::to_string(ordinal) + "/" + std::to_string(search.matches.size());
}

void Application::scrollToCurrentMatch() {
    if (search_.current >= search_.matches.size()) return;
    const std::size_t line = search_.matches[search_.current].line;
    // Keep the match two rows from the top so its context stays visible.
    const std::size_t desired = line > 2 ? line - 2 : 0;
    const std::size_t maximum = maximumContentScroll();
    contentState_.scroll = std::min(desired, maximum);
}

void Application::stepSearch(int direction) {
    if (!search_.hasMatches()) {
        searchStatus_ = "no matches";
        return;
    }
    if (!search::seekFrom(search_, contentState_.scroll, direction)) return;
    searchStatus_ = matchPosition(search_);
    scrollToCurrentMatch();
}

void Application::endSearch(bool commit) {
    searchMode_ = false;
    if (!commit || search_.query.empty()) {
        cancelSearch();
        return;
    }
    if (search_.matches.empty()) {
        setMessage("no matches for \"" + search_.query.text + "\"", true);
        search_.query.text.clear();
        search_.matches.clear();
        return;
    }
    // Keep the query and highlight so n / N keep working after Enter.
    if (search_.current == static_cast<std::size_t>(-1)) search_.current = 0;
    searchStatus_ = matchPosition(search_);
    scrollToCurrentMatch();
    setMessage("/" + search_.query.text + "  " + searchStatus_ + "   (n/N to move)", false);
}

void Application::cancelSearch() {
    searchMode_ = false;
    search_ = search::SearchState{};
    searchStatus_.clear();
}

void Application::handleSearchKey(const terminal::KeyEvent& event) {
    if (event.key == terminal::Key::Escape) {
        cancelSearch();
        return;
    }
    if (event.key == terminal::Key::Enter) {
        endSearch(/*commit=*/true);
        return;
    }
    if (event.key == terminal::Key::Backspace || event.key == terminal::Key::Delete) {
        std::string& query = search_.query.text;
        if (!query.empty()) {
            // Trim one full UTF-8 code point so multi-byte input edits cleanly.
            std::size_t cut = query.size() - 1;
            while (cut > 0 && (static_cast<unsigned char>(query[cut]) & 0xC0) == 0x80) --cut;
            query.resize(cut);
        }
        refreshSearch();
        return;
    }
    if (event.key == terminal::Key::Up) {
        stepSearch(-1);
        return;
    }
    if (event.key == terminal::Key::Down) {
        stepSearch(1);
        return;
    }
    if (event.key == terminal::Key::Tab) {
        endSearch(/*commit=*/true);
        return;
    }
    if (event.key == terminal::Key::CtrlC || event.key == terminal::Key::CtrlU) {
        if (event.key == terminal::Key::CtrlC) {
            quit_ = true;
            return;
        }
        search_.query.text.clear();
        refreshSearch();
        return;
    }
    if (event.isCharacter()) {
        search_.query.text += event.text();
        refreshSearch();
        if (!search_.matches.empty()) {
            // Incremental: land on the first match from the viewport as the
            // query grows, without waiting for Enter.
            if (search_.current == static_cast<std::size_t>(-1)) {
                stepSearch(1);
            } else {
                const std::size_t keep = search_.matches[search_.current].line;
                search::seekToFirstAtOrAfter(search_, keep);
                scrollToCurrentMatch();
            }
        }
        return;
    }
    // Everything else (arrows handled above, function keys, ...) is ignored
    // while typing the query.
}

void Application::moveSelection(int delta) {
    if (focus_ == Pane::Content) {
        scrollContent(delta * 3);
        return;
    }
    const auto& nodes = tree_.visibleNodes();
    if (nodes.empty()) return;
    const long next = static_cast<long>(treeState_.selected) + delta;
    const long last = static_cast<long>(nodes.size()) - 1;
    treeState_.selected = static_cast<std::size_t>(std::clamp<long>(next, 0, std::max<long>(0, last)));
}

void Application::moveByPage(int direction) {
    const int rows = focus_ == Pane::Content ? std::max(1, layout_.content.height - 1)
                                             : std::max(1, layout_.tree.height - 1);
    if (focus_ == Pane::Content) {
        scrollContent(direction * rows);
    } else {
        moveSelection(direction * rows);
    }
}

void Application::jumpToStart() {
    if (focus_ == Pane::Content) {
        contentState_.scroll = 0;
    } else {
        treeState_.selected = 0;
        treeState_.scroll = 0;
    }
}

void Application::jumpToEnd() {
    if (focus_ == Pane::Content) {
        contentState_.scroll = maximumContentScroll();
    } else {
        const auto& nodes = tree_.visibleNodes();
        if (!nodes.empty()) treeState_.selected = nodes.size() - 1;
    }
}

void Application::activateSelection() {
    if (focus_ == Pane::Content) return;
    const auto& nodes = tree_.visibleNodes();
    if (treeState_.selected >= nodes.size()) return;

    filesystem::TreeNode& node = *nodes[treeState_.selected];
    if (node.isDirectory) {
        tree_.setExpanded(node, !node.expanded);
        return;
    }
    if (!node.readable) return;
    openFile(node.path);
    focus_ = Pane::Content;
}

void Application::expandSelection() {
    if (focus_ == Pane::Content) {
        scrollContentHorizontal(4);
        return;
    }
    const auto& nodes = tree_.visibleNodes();
    if (treeState_.selected >= nodes.size()) return;
    filesystem::TreeNode& node = *nodes[treeState_.selected];
    if (node.isDirectory) {
        tree_.expand(node);
    } else if (node.parent != nullptr) {
        // Right on a file moves into the viewer, which is what a user expects.
        openFile(node.path);
        focus_ = Pane::Content;
    }
}

void Application::collapseSelection() {
    if (focus_ == Pane::Content) {
        scrollContentHorizontal(-4);
        return;
    }
    const auto& nodes = tree_.visibleNodes();
    if (treeState_.selected >= nodes.size()) return;
    filesystem::TreeNode& node = *nodes[treeState_.selected];
    if (node.isDirectory && node.expanded) {
        tree_.collapse(node);
        return;
    }
    if (node.parent != nullptr) {
        const std::size_t index = tree_.indexOf(node.parent->path);
        if (index != static_cast<std::size_t>(-1)) treeState_.selected = index;
    }
}

void Application::scrollContent(int deltaLines) {
    if (rendered_.lines.empty()) return;
    const long next = static_cast<long>(contentState_.scroll) + deltaLines;
    const long last = static_cast<long>(maximumContentScroll());
    contentState_.scroll = static_cast<std::size_t>(std::clamp<long>(next, 0, std::max<long>(0, last)));
}

void Application::scrollContentHorizontal(int deltaColumns) {
    if (wrap_) return;
    const int overflow = std::max(0, ui::widestLine(rendered_) - contentWidth());
    contentState_.horizontalScroll =
        std::clamp(contentState_.horizontalScroll + deltaColumns, 0, overflow);
}

void Application::jumpToHeading(int direction) {
    if (rendered_.headingLines.empty()) return;
    const auto& headings = rendered_.headingLines;
    std::size_t target = 0;
    if (direction > 0) {
        const auto it = std::upper_bound(headings.begin(), headings.end(), contentState_.scroll);
        target = it == headings.end() ? headings.back() : *it;
    } else {
        const auto it = std::lower_bound(headings.begin(), headings.end(), contentState_.scroll);
        if (it == headings.begin()) {
            target = headings.front();
        } else {
            target = *(it - 1);
        }
    }
    contentState_.scroll = std::min(target, maximumContentScroll());
    focus_ = Pane::Content;
}

void Application::toggleWrap() {
    wrap_ = !wrap_;
    renderDocument();
    contentState_.horizontalScroll = 0;
    setMessage(wrap_ ? "word wrap on" : "word wrap off", false);
}

void Application::toggleHidden() {
    tree_.setShowHidden(!tree_.showHidden());
    treeState_.selected = std::min(treeState_.selected, tree_.visibleNodes().size() - 1);
    setMessage(tree_.showHidden() ? "showing hidden files" : "hiding hidden files", false);
}

void Application::toggleTree() {
    showTree_ = !showTree_;
    if (!showTree_ && focus_ == Pane::Tree) focus_ = Pane::Content;
    setMessage(showTree_ ? "file tree shown" : "file tree hidden", false);
}

void Application::handleKey(const terminal::KeyEvent& event) {
    if (event.key == terminal::Key::None) return;

    if (searchMode_) {
        handleSearchKey(event);
        return;
    }

    if (showHelp_) {
        showHelp_ = false;
        screen_.invalidate();
        return;
    }

    switch (event.key) {
        case terminal::Key::CtrlC:
        case terminal::Key::CtrlD:
        case terminal::Key::CtrlZ:
            quit_ = true;
            return;
        case terminal::Key::CtrlL:
            screen_.invalidate();
            return;
        case terminal::Key::CtrlU:
            moveByPage(-1);
            return;
        case terminal::Key::CtrlW:
            return;
        case terminal::Key::CtrlR:
            reloadCurrentFile();
            return;
        case terminal::Key::Tab:
        case terminal::Key::BackTab:
            cycleFocus();
            return;
        case terminal::Key::Escape:
            return;
        case terminal::Key::Up:
            moveSelection(-1);
            return;
        case terminal::Key::Down:
            moveSelection(1);
            return;
        case terminal::Key::Left:
            collapseSelection();
            return;
        case terminal::Key::Right:
            expandSelection();
            return;
        case terminal::Key::PageUp:
            moveByPage(-1);
            return;
        case terminal::Key::PageDown:
            moveByPage(1);
            return;
        case terminal::Key::Home:
            jumpToStart();
            return;
        case terminal::Key::End:
            jumpToEnd();
            return;
        case terminal::Key::Enter:
            activateSelection();
            return;
        case terminal::Key::Backspace:
        case terminal::Key::Delete:
        case terminal::Key::Insert:
        case terminal::Key::None:
        case terminal::Key::Character:
        case terminal::Key::F1:
        case terminal::Key::F2:
        case terminal::Key::F3:
        case terminal::Key::F4:
        case terminal::Key::F5:
        case terminal::Key::F6:
        case terminal::Key::F7:
        case terminal::Key::F8:
        case terminal::Key::F9:
        case terminal::Key::F10:
        case terminal::Key::F11:
        case terminal::Key::F12:
            break;
    }

    if (!event.isCharacter()) return;

    if (event.isPlainChar('q')) {
        quit_ = true;
        return;
    }
    if (event.isPlainChar('?')) {
        showHelp_ = true;
        return;
    }
    if (event.isPlainChar('j')) {
        moveSelection(1);
        return;
    }
    if (event.isPlainChar('k')) {
        moveSelection(-1);
        return;
    }
    if (event.isPlainChar('h')) {
        collapseSelection();
        return;
    }
    if (event.isPlainChar('l')) {
        expandSelection();
        return;
    }
    if (event.isPlainChar('g')) {
        jumpToStart();
        return;
    }
    if (event.isPlainChar('G')) {
        jumpToEnd();
        return;
    }
    if (event.isPlainChar('r')) {
        reloadCurrentFile();
        return;
    }
    if (event.isPlainChar('R')) {
        tree_.refresh();
        selectPathInTree(currentPath_);
        setMessage("file tree refreshed", false);
        return;
    }
    if (event.isPlainChar('w')) {
        toggleWrap();
        return;
    }
    if (event.isPlainChar('.')) {
        toggleHidden();
        return;
    }
    if (event.isPlainChar('t')) {
        toggleTree();
        return;
    }
    if (event.isPlainChar('/') || event.isPlainChar('s')) {
        beginSearch();
        return;
    }
    if (event.isPlainChar('n')) {
        stepSearch(1);
        if (!search_.hasMatches()) setMessage("no search to repeat", true);
        return;
    }
    if (event.isPlainChar('N')) {
        stepSearch(-1);
        if (!search_.hasMatches()) setMessage("no search to repeat", true);
        return;
    }
    if (event.isPlainChar(']')) {
        jumpToHeading(1);
        return;
    }
    if (event.isPlainChar('[')) {
        jumpToHeading(-1);
        return;
    }
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void Application::render() {
    const terminal::TerminalSize size = terminal_->size();
    if (size.width != lastSize_.width || size.height != lastSize_.height) {
        lastSize_ = size;
        screen_.resize(size.width, size.height);
        screen_.invalidate();
        renderDocument();
    }
    if (renderedWidth_ != contentWidth()) {
        renderDocument();
    }

    layout_ = computeLayout(screen_.bounds(), layoutOptions());
    screen_.clear(theme_.text);

    // --- header ----------------------------------------------------------
    ui::HeaderInfo header;
    header.title = currentTitle_.empty() ? std::string("mdview") : currentTitle_;
    header.path = currentPath_.empty() ? std::string()
                                       : filesystem::displayPath(currentPath_, tree_.root());
    header.details = headerDetails();
    ui::drawHeaderBar(screen_, layout_.header, header, theme_);

    // --- tree ------------------------------------------------------------
    if (!layout_.tree.empty()) {
        ui::scrollTreeToSelection(treeState_, tree_.visibleNodes().size(), layout_.tree.height);
        ui::drawTreeView(screen_, layout_.tree, tree_, treeState_, theme_, capabilities_,
                         focus_ == Pane::Tree);
    }
    if (!layout_.divider.empty()) {
        screen_.pushClip(layout_.divider);
        screen_.drawVLine(layout_.divider.x, layout_.divider.y, layout_.divider.height,
                          capabilities_.unicode ? "\xE2\x94\x82" : "|",
                          focus_ == Pane::Tree ? theme_.borderFocused : theme_.border);
        screen_.popClip();
    }

    // --- document --------------------------------------------------------
    ui::clampMarkdownScroll(contentState_, rendered_, layout_.content, contentWidth(), wrap_);
    ui::drawMarkdownView(screen_, layout_.content, rendered_, contentState_, theme_, capabilities_,
                         focus_ == Pane::Content, emptyMessage());
    ui::paintSearchHighlights(screen_, layout_.content, rendered_, contentState_, search_, theme_);

    // --- status ----------------------------------------------------------
    std::vector<ui::StatusHint> hints;
    if (focus_ == Pane::Tree) {
        hints = {{"Enter", "open"}, {"Tab", "document"}, {"R", "refresh"}, {"?", "help"}, {"q", "quit"}};
    } else {
        hints = {{"Up/Down", "scroll"}, {"[ ]", "headings"}, {"w", "wrap"}, {"Tab", "tree"}, {"?", "help"}};
    }

    ui::StatusBarInfo status;
    status.hints = std::move(hints);
    status.right = rightStatus();
    status.message = message_;
    status.isError = messageIsError_;
    if (searchMode_) {
        status.searchPrompt = "/" + search_.query.text + "_";
        status.right = searchStatus_;
    }
    ui::drawStatusBar(screen_, layout_.status, status, theme_);

    if (showHelp_) {
        ui::drawHelpOverlay(screen_, screen_.bounds(), theme_, capabilities_);
    }

    screen_.flush(*terminal_);
}

int Application::run() {
    initialize();

    bool dirty = true;
    while (!quit_) {
        const terminal::KeyEvent event = terminal_->readKey();
        if (event.key != terminal::Key::None) {
            handleKey(event);
            dirty = true;
        }

        const terminal::TerminalSize size = terminal_->size();
        if (size.width != lastSize_.width || size.height != lastSize_.height) {
            dirty = true;
        }

        if (watcher_) {
            if (watcher_->changed()) {
                watcher_->takeChanged();
                reloadCurrentFile();
                dirty = true;
            }
        }

        if (dirty) {
            render();
            dirty = false;

            // Rendering may have resized the screen (first frame, terminal
            // resize); make sure the reported size is current for next time.
            const terminal::TerminalSize after = terminal_->size();
            if (after.width != lastSize_.width || after.height != lastSize_.height) {
                dirty = true;
            }
        }
    }

    shutdown();
    return 0;
}

}  // namespace mdview::app
