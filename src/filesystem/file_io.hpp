#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

// File reading and path helpers.
//
// Everything here goes through <filesystem> and the standard library; there is
// no CreateFile/FindFirstFile/GetFileAttributes anywhere in the application
// architecture, which is what makes the tree and the loader portable by
// construction.

namespace mdview::filesystem {

/// Files above this size are not loaded into the viewer; an 8 MB Markdown
/// document is already far beyond what a terminal can usefully scroll.
constexpr std::uintmax_t kMaxTextFileBytes = 8ull * 1024ull * 1024ull;

struct ReadResult {
    bool ok = false;
    std::string text;
    std::string error;
    std::uintmax_t size = 0;
};

ReadResult readTextFile(const std::filesystem::path& path);

/// True for the extensions mdview renders as Markdown.
bool isMarkdownPath(const std::filesystem::path& path);

/// Heuristic binary check used before handing a file to the parser.
bool looksBinary(std::string_view head);

/// "12 B", "1.4 KB", "3.1 MB".
std::string humanReadableSize(std::uintmax_t bytes);

/// Path relative to `root` when it is inside it, otherwise the plain path.
std::string displayPath(const std::filesystem::path& path, const std::filesystem::path& root);

/// Lower-cased extension without the dot, e.g. "md" or "cpp".
std::string extensionLabel(const std::filesystem::path& path);

/// A short, human friendly name for a directory, e.g. the last component of
/// "/home/user/project" or "C:\Users\user\project".
std::string directoryName(const std::filesystem::path& path);

}  // namespace mdview::filesystem
