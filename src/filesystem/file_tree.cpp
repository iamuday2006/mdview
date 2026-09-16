#include "filesystem/file_tree.hpp"

#include "filesystem/file_io.hpp"
#include "utils/string_utils.hpp"

#include <algorithm>
#include <system_error>

namespace mdview::filesystem {

namespace fs = std::filesystem;

namespace {

bool entryLess(const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
    // Directories first, then case-insensitive by name, with a stable
    // tie-break so the order never jitters between refreshes.
    if (a->isDirectory != b->isDirectory) return a->isDirectory;
    const int comparison = str::compareIgnoreCaseAscii(a->name, b->name);
    if (comparison != 0) return comparison < 0;
    return a->name < b->name;
}

}  // namespace

void FileTree::setRoot(const fs::path& root) {
    std::error_code error;
    rootPath_ = fs::weakly_canonical(root, error);
    if (error || rootPath_.empty()) rootPath_ = root;
    reload();
}

void FileTree::setShowHidden(bool show) {
    if (showHidden_ == show) return;
    showHidden_ = show;
    reload();
}

void FileTree::reload() {
    std::vector<fs::path> expanded;
    if (root_) collectExpanded(*root_, expanded);

    root_ = std::make_unique<TreeNode>();
    root_->path = rootPath_;
    root_->name = directoryName(rootPath_);
    root_->isDirectory = true;
    root_->expanded = true;
    root_->loaded = false;
    root_->depth = 0;

    std::error_code error;
    if (!fs::exists(rootPath_, error)) {
        root_->readable = false;
        invalidate();
        return;
    }

    loadChildren(*root_);
    root_->expanded = true;
    // The root itself is never recorded as "expanded" (it always is), so the
    // restore starts one level down.
    for (auto& child : root_->children) {
        if (child->isDirectory) restoreExpanded(*child, expanded, 1);
    }
    invalidate();
}

void FileTree::refresh() { reload(); }

void FileTree::collectExpanded(const TreeNode& node, std::vector<fs::path>& out) const {
    if (node.isDirectory && node.expanded && node.parent != nullptr) out.push_back(node.path);
    for (const auto& child : node.children) {
        if (child->isDirectory && child->expanded) collectExpanded(*child, out);
    }
}

void FileTree::restoreExpanded(TreeNode& node, const std::vector<fs::path>& expanded, int depth) {
    if (depth >= kMaxDepth) return;
    if (std::find(expanded.begin(), expanded.end(), node.path) == expanded.end()) return;

    loadChildren(node);
    node.expanded = true;
    for (auto& child : node.children) {
        if (child->isDirectory) restoreExpanded(*child, expanded, depth + 1);
    }
}

void FileTree::loadChildren(TreeNode& node) {
    node.children.clear();
    node.loaded = true;

    if (!node.isDirectory || node.depth >= kMaxDepth) return;

    std::error_code error;
    fs::directory_iterator iterator(node.path, fs::directory_options::skip_permission_denied, error);
    if (error) {
        node.readable = false;
        return;
    }

    const fs::directory_iterator end;
    while (iterator != end) {
        // Copy the entry: increment() invalidates the reference returned by
        // operator*, and the reads below happen after we advance.
        const fs::directory_entry entry = *iterator;
        iterator.increment(error);
        if (error) {
            error.clear();
            break;
        }

        std::error_code entryError;
        const bool isSymlink = entry.is_symlink(entryError);
        entryError.clear();
        const bool isDirectory = entry.is_directory(entryError);
        entryError.clear();
        const bool isRegular = entry.is_regular_file(entryError);
        if (!isDirectory && !isRegular) continue;

        const std::string name = entry.path().filename().string();
        if (name.empty()) continue;
        if (!showHidden_ && name.front() == '.') continue;
        // Skip the pseudo entries that some filesystems report.
        if (name == "." || name == "..") continue;

        auto child = std::make_unique<TreeNode>();
        child->path = entry.path();
        child->name = name;
        child->isDirectory = isDirectory;
        child->isSymlink = isSymlink;
        child->isMarkdown = !isDirectory && isMarkdownPath(entry.path());
        child->parent = &node;
        child->depth = node.depth + 1;
        if (!isDirectory) {
            std::error_code sizeError;
            child->size = entry.file_size(sizeError);
        }
        node.children.push_back(std::move(child));
    }

    std::sort(node.children.begin(), node.children.end(), entryLess);
}

void FileTree::expand(TreeNode& node) {
    if (!node.isDirectory) return;
    if (!node.loaded) loadChildren(node);
    node.expanded = true;
    invalidate();
}

void FileTree::collapse(TreeNode& node) {
    node.expanded = false;
    invalidate();
}

void FileTree::setExpanded(TreeNode& node, bool expanded) {
    if (expanded) {
        expand(node);
    } else {
        collapse(node);
    }
}

void FileTree::appendVisible(TreeNode& node, std::vector<TreeNode*>& out) const {
    out.push_back(&node);
    if (!node.isDirectory || !node.expanded) return;
    for (auto& child : node.children) appendVisible(*child, out);
}

const std::vector<TreeNode*>& FileTree::visibleNodes() const {
    if (!visibleValid_) {
        visible_.clear();
        if (root_) appendVisible(*root_, visible_);
        visibleValid_ = true;
    }
    return visible_;
}

std::size_t FileTree::indexOf(const fs::path& path) const {
    const auto& nodes = visibleNodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i]->path == path) return i;
    }
    return static_cast<std::size_t>(-1);
}

TreeNode* FileTree::find(const fs::path& path) {
    std::error_code error;
    const fs::path normalised = fs::weakly_canonical(path, error);
    const fs::path& wanted = error ? path : normalised;

    const auto& nodes = visibleNodes();
    for (TreeNode* node : nodes) {
        if (node->path == wanted) return node;
    }
    return nullptr;
}

}  // namespace mdview::filesystem
