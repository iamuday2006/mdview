#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// The navigable filesystem tree.
//
// Children are loaded lazily: opening a large directory tree does not walk the
// whole disk, only the directories the user actually expands.  Everything is
// built on std::filesystem::directory_iterator, so the model is identical on
// Windows, Linux and macOS.

namespace mdview::filesystem {

struct TreeNode {
    std::filesystem::path path;
    std::string name;
    bool isDirectory = false;
    bool isSymlink = false;
    bool isMarkdown = false;
    bool expanded = false;
    bool loaded = false;
    bool readable = true;
    std::uintmax_t size = 0;
    int depth = 0;
    TreeNode* parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;
};

class FileTree {
public:
    /// Maximum nesting depth that will ever be loaded, which keeps a symlink
    /// cycle from turning into an infinite walk.
    static constexpr int kMaxDepth = 32;

    FileTree() = default;

    void setRoot(const std::filesystem::path& root);
    const std::filesystem::path& root() const { return rootPath_; }

    TreeNode* rootNode() { return root_.get(); }
    const TreeNode* rootNode() const { return root_.get(); }

    void setShowHidden(bool show);
    bool showHidden() const { return showHidden_; }

    /// Re-reads the tree from disk while preserving which directories were
    /// open and which entries were marked.
    void refresh();

    void expand(TreeNode& node);
    void collapse(TreeNode& node);
    void setExpanded(TreeNode& node, bool expanded);

    /// Depth-first list of the rows that should currently be drawn.
    const std::vector<TreeNode*>& visibleNodes() const;

    /// Position of `path` in visibleNodes(), or npos.
    std::size_t indexOf(const std::filesystem::path& path) const;

    /// Finds the node for `path` by walking the loaded part of the tree.
    TreeNode* find(const std::filesystem::path& path);

private:
    void reload();
    void loadChildren(TreeNode& node);
    void appendVisible(TreeNode& node, std::vector<TreeNode*>& out) const;
    void collectExpanded(const TreeNode& node, std::vector<std::filesystem::path>& out) const;
    void restoreExpanded(TreeNode& node, const std::vector<std::filesystem::path>& expanded, int depth);
    void invalidate() { visibleValid_ = false; }

    std::filesystem::path rootPath_;
    std::unique_ptr<TreeNode> root_;
    bool showHidden_ = false;

    mutable std::vector<TreeNode*> visible_;
    mutable bool visibleValid_ = false;
};

}  // namespace mdview::filesystem
