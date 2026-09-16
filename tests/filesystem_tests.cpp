#include "test_framework.hpp"
#include "temp_directory.hpp"

#include "filesystem/file_io.hpp"
#include "filesystem/file_tree.hpp"
#include "filesystem/file_watcher.hpp"
#include "utils/time.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace mdview;
using mdview::test::TempDirectory;
namespace stdfs = std::filesystem;

// ---------------------------------------------------------------------------
// file_io
// ---------------------------------------------------------------------------

MDVIEW_TEST(file_io, reads_text_files) {
    TempDirectory temp("read");
    temp.write("doc.md", "# Title\n\nbody\n");

    const filesystem::ReadResult result = filesystem::readTextFile(temp.path() / "doc.md");
    CHECK(result.ok);
    CHECK_EQ(result.text, std::string("# Title\n\nbody\n"));
    CHECK_EQ(result.size, static_cast<std::uintmax_t>(14));
    CHECK(result.error.empty());
}

MDVIEW_TEST(file_io, reports_missing_files_and_directories) {
    TempDirectory temp("missing");
    const filesystem::ReadResult missing = filesystem::readTextFile(temp.path() / "nope.md");
    CHECK(!missing.ok);
    CHECK(!missing.error.empty());

    const filesystem::ReadResult directory = filesystem::readTextFile(temp.path());
    CHECK(!directory.ok);
    CHECK_EQ(directory.error, std::string("is a directory"));
}

MDVIEW_TEST(file_io, strips_the_utf8_bom) {
    TempDirectory temp("bom");
    std::string content = "\xEF\xBB\xBF# Title\n";
    temp.write("bom.md", content);

    const filesystem::ReadResult result = filesystem::readTextFile(temp.path() / "bom.md");
    CHECK(result.ok);
    CHECK_EQ(result.text, std::string("# Title\n"));
}

MDVIEW_TEST(file_io, refuses_binary_files) {
    TempDirectory temp("binary");
    std::string content = "abc";
    content.push_back('\0');
    content += "def";
    temp.write("bin.dat", content);

    const filesystem::ReadResult result = filesystem::readTextFile(temp.path() / "bin.dat");
    CHECK(!result.ok);
    CHECK_EQ(result.error, std::string("binary file"));
}

MDVIEW_TEST(file_io, detects_markdown_paths) {
    CHECK(filesystem::isMarkdownPath("a.md"));
    CHECK(filesystem::isMarkdownPath("a.MARKDOWN"));
    CHECK(filesystem::isMarkdownPath("a.mdx"));
    CHECK(!filesystem::isMarkdownPath("a.txt"));
    CHECK(!filesystem::isMarkdownPath("Makefile"));
}

MDVIEW_TEST(file_io, formats_sizes) {
    CHECK_EQ(filesystem::humanReadableSize(0), std::string("0 B"));
    CHECK_EQ(filesystem::humanReadableSize(512), std::string("512 B"));
    CHECK_EQ(filesystem::humanReadableSize(1024), std::string("1.0 KB"));
    CHECK_EQ(filesystem::humanReadableSize(1536), std::string("1.5 KB"));
    CHECK_EQ(filesystem::humanReadableSize(1024 * 1024), std::string("1.0 MB"));
}

MDVIEW_TEST(file_io, path_helpers) {
    CHECK_EQ(filesystem::extensionLabel("notes.md"), std::string("md"));
    CHECK_EQ(filesystem::extensionLabel("Makefile"), std::string(""));

    const stdfs::path root = stdfs::path("root") / "sub";
    CHECK_EQ(filesystem::displayPath(root / "file.md", root), std::string("file.md"));
    CHECK_EQ(filesystem::directoryName("/home/user/project"), std::string("project"));
}

// ---------------------------------------------------------------------------
// FileTree
// ---------------------------------------------------------------------------

MDVIEW_TEST(file_tree, lists_directories_first_then_files) {
    TempDirectory temp("tree");
    temp.makeDirectory("zeta");
    temp.makeDirectory("Alpha");
    temp.write("b.md", "# b\n");
    temp.write("a.md", "# a\n");

    filesystem::FileTree tree;
    tree.setRoot(temp.path());

    const std::vector<filesystem::TreeNode*>& nodes = tree.visibleNodes();
    REQUIRE(nodes.size() == 5);
    CHECK(nodes[0]->isDirectory);   // the root is a node too,
    CHECK(nodes[0]->expanded);      // expanded by default
    CHECK(nodes[1]->isDirectory);
    CHECK_EQ(nodes[1]->name, std::string("Alpha"));      // case-insensitive ordering
    CHECK(nodes[2]->isDirectory);
    CHECK_EQ(nodes[2]->name, std::string("zeta"));
    CHECK_EQ(nodes[3]->name, std::string("a.md"));
    CHECK(nodes[3]->isMarkdown);
    CHECK(!nodes[4]->isDirectory);
}

MDVIEW_TEST(file_tree, hides_dotfiles_until_asked) {
    TempDirectory temp("hidden");
    temp.write(".secret", "shh");
    temp.write("visible.md", "# v\n");

    filesystem::FileTree tree;
    tree.setRoot(temp.path());
    CHECK(!tree.showHidden());
    CHECK_EQ(tree.visibleNodes().size(), std::size_t{2});

    tree.setShowHidden(true);
    CHECK_EQ(tree.visibleNodes().size(), std::size_t{3});
}

MDVIEW_TEST(file_tree, expands_and_collapses_lazily) {
    TempDirectory temp("expand");
    temp.makeDirectory("sub");
    temp.write("sub/inner.md", "# inner\n");

    filesystem::FileTree tree;
    tree.setRoot(temp.path());

    filesystem::TreeNode* sub = nullptr;
    for (filesystem::TreeNode* node : tree.visibleNodes()) {
        if (node->name == "sub") sub = node;
    }
    REQUIRE(sub != nullptr);
    CHECK(!sub->loaded);

    const std::size_t collapsed = tree.visibleNodes().size();
    tree.expand(*sub);
    CHECK(sub->loaded);
    CHECK_EQ(tree.visibleNodes().size(), collapsed + 1);

    tree.collapse(*sub);
    CHECK_EQ(tree.visibleNodes().size(), collapsed);
}

MDVIEW_TEST(file_tree, find_and_index_locate_nodes) {
    TempDirectory temp("find");
    temp.write("doc.md", "# doc\n");

    filesystem::FileTree tree;
    tree.setRoot(temp.path());

    const stdfs::path document = temp.path() / "doc.md";
    const std::size_t index = tree.indexOf(document);
    CHECK(index != static_cast<std::size_t>(-1));
    REQUIRE(index < tree.visibleNodes().size());
    CHECK(tree.visibleNodes()[index]->path == document);
    CHECK(tree.find(document) != nullptr);
}

MDVIEW_TEST(file_tree, refresh_keeps_expansion) {
    TempDirectory temp("refresh");
    temp.makeDirectory("sub");
    temp.write("sub/inner.md", "# inner\n");

    filesystem::FileTree tree;
    tree.setRoot(temp.path());

    filesystem::TreeNode* sub = nullptr;
    for (filesystem::TreeNode* node : tree.visibleNodes()) {
        if (node->name == "sub") sub = node;
    }
    REQUIRE(sub != nullptr);
    tree.expand(*sub);
    const std::size_t expanded = tree.visibleNodes().size();

    tree.refresh();
    CHECK_EQ(tree.visibleNodes().size(), expanded);
}

// ---------------------------------------------------------------------------
// FileWatcher
// ---------------------------------------------------------------------------

MDVIEW_TEST(watcher, polling_detects_a_modification) {
    TempDirectory temp("watch");
    const stdfs::path document = temp.path() / "doc.md";
    temp.write("doc.md", "# one\n");

    const std::unique_ptr<filesystem::FileWatcher> watcher = filesystem::createFileWatcher();
    REQUIRE(watcher != nullptr);
    watcher->watch(document);

    // The first poll just primes the snapshot.
    CHECK(!watcher->changed());

    temp.write("doc.md", "# two, longer content\n");

    std::uint64_t waited = 0;
    while (!watcher->changed() && waited < 5000) {
        sleepMillis(120);
        waited += 120;
    }
    CHECK(watcher->changed());

    const std::vector<stdfs::path> changed = watcher->takeChanged();
    CHECK_EQ(changed.size(), std::size_t{1});
    CHECK(!watcher->changed());
}

MDVIEW_TEST(watcher, unwatch_stops_reporting) {
    TempDirectory temp("unwatch");
    const stdfs::path document = temp.path() / "doc.md";
    temp.write("doc.md", "# one\n");

    const std::unique_ptr<filesystem::FileWatcher> watcher = filesystem::createFileWatcher();
    watcher->watch(document);
    CHECK(!watcher->changed());
    watcher->unwatch(document);

    temp.write("doc.md", "# changed content\n");
    sleepMillis(500);
    CHECK(!watcher->changed());
}
