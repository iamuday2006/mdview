#include "filesystem/file_io.hpp"

#include "utils/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>

namespace mdview::filesystem {

namespace fs = std::filesystem;

ReadResult readTextFile(const fs::path& path) {
    ReadResult result;

    std::error_code error;
    const fs::file_status status = fs::status(path, error);
    if (error || !fs::exists(status)) {
        result.error = "file not found";
        return result;
    }
    if (fs::is_directory(status)) {
        result.error = "is a directory";
        return result;
    }

    result.size = fs::file_size(path, error);
    if (error) result.size = 0;

    if (result.size > kMaxTextFileBytes) {
        result.error = "file is too large to display (" + humanReadableSize(result.size) + ")";
        return result;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        result.error = "could not open file";
        return result;
    }

    std::string text;
    text.resize(static_cast<std::size_t>(result.size));
    if (result.size > 0) {
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
        const std::streamsize actuallyRead = stream.gcount();
        text.resize(static_cast<std::size_t>(actuallyRead < 0 ? 0 : actuallyRead));
    }
    if (stream.bad()) {
        result.error = "error while reading file";
        return result;
    }

    const std::string_view head(text.data(), std::min<std::size_t>(text.size(), 8192));
    if (looksBinary(head)) {
        result.error = "binary file";
        return result;
    }

    // Normalise a UTF-8 BOM away so the first heading is not prefixed by it.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    result.text = std::move(text);
    result.ok = true;
    return result;
}

bool isMarkdownPath(const fs::path& path) {
    static const char* const kExtensions[] = {".md",   ".markdown", ".mdown", ".mkd",
                                              ".mkdn", ".mdx",      ".mdwn",  ".mdtxt"};
    std::string extension = path.extension().string();
    if (extension.empty()) return false;
    for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* candidate : kExtensions) {
        if (extension == candidate) return true;
    }
    return false;
}

bool looksBinary(std::string_view head) {
    if (head.empty()) return false;
    std::size_t suspicious = 0;
    for (const char raw : head) {
        const unsigned char byte = static_cast<unsigned char>(raw);
        if (byte == 0x00) return true;
        // Control characters other than tab/newline/carriage return/form feed.
        if (byte < 0x09 || (byte > 0x0D && byte < 0x20)) ++suspicious;
    }
    return suspicious * 100 > head.size();  // > 1% control bytes
}

std::string humanReadableSize(std::uintmax_t bytes) {
    static const char* const kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(kUnits)) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0) return std::to_string(bytes) + " B";

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, kUnits[unit]);
    return buffer;
}

std::string displayPath(const fs::path& path, const fs::path& root) {
    std::error_code error;
    const fs::path relative = fs::relative(path, root, error);
    if (!error && !relative.empty() && relative.string().rfind("..", 0) != 0) {
        return relative.generic_string();
    }
    return path.generic_string();
}

std::string extensionLabel(const fs::path& path) {
    std::string extension = path.extension().string();
    if (!extension.empty() && extension.front() == '.') extension.erase(0, 1);
    return str::toLowerAscii(extension);
}

std::string directoryName(const fs::path& path) {
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(path, error);
    const fs::path& resolved = error ? path : canonical;

    std::string name = resolved.filename().string();
    if (name.empty()) {
        // Root-like path ("/", "C:\"): fall back to the generic form.
        name = resolved.generic_string();
        if (name.size() > 1 && name.back() == '/') name.pop_back();
        if (name.empty()) name = "/";
    }
    return name;
}

}  // namespace mdview::filesystem
