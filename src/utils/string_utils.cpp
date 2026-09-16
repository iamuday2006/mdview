#include "utils/string_utils.hpp"

#include <algorithm>

namespace mdview::str {

namespace {

inline bool isAsciiSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

inline char toLowerChar(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string_view trimLeft(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size() && isAsciiSpace(text[i])) ++i;
    return text.substr(i);
}

std::string_view trimRight(std::string_view text) {
    std::size_t n = text.size();
    while (n > 0 && isAsciiSpace(text[n - 1])) --n;
    return text.substr(0, n);
}

std::string_view trim(std::string_view text) { return trimRight(trimLeft(text)); }

bool isBlank(std::string_view text) { return trimLeft(text).empty(); }

std::string toLowerAscii(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = toLowerChar(c);
    return out;
}

std::string toUpperAscii(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b) {
    return compareIgnoreCaseAscii(a, b) == 0;
}

int compareIgnoreCaseAscii(std::string_view a, std::string_view b) {
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char ca = static_cast<unsigned char>(toLowerChar(a[i]));
        const unsigned char cb = static_cast<unsigned char>(toLowerChar(b[i]));
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    if (a.size() == b.size()) return 0;
    return a.size() < b.size() ? -1 : 1;
}

std::vector<std::string_view> split(std::string_view text, char delimiter, bool keepEmpty) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = text.find(delimiter, start);
        const std::size_t end = pos == std::string_view::npos ? text.size() : pos;
        const std::string_view piece = text.substr(start, end - start);
        if (keepEmpty || !piece.empty()) parts.push_back(piece);
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return parts;
}

std::size_t findLineBreak(std::string_view text, std::size_t from, std::size_t& lineEnd) {
    std::size_t i = from;
    while (i < text.size() && text[i] != '\n' && text[i] != '\r') ++i;
    lineEnd = i;
    if (i >= text.size()) return text.size();
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') return i + 2;
    return i + 1;
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t lineEnd = 0;
        const std::size_t next = findLineBreak(text, start, lineEnd);
        lines.emplace_back(text.substr(start, lineEnd - start));
        start = next;
    }
    return lines;
}

std::string replaceAll(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(text);
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text.compare(i, from.size(), from) == 0) {
            out.append(to);
            i += from.size();
        } else {
            out.push_back(text[i]);
            ++i;
        }
    }
    return out;
}

std::string repeat(std::string_view text, std::size_t count) {
    std::string out;
    out.reserve(text.size() * count);
    for (std::size_t i = 0; i < count; ++i) out.append(text);
    return out;
}

std::string padLeftAscii(std::string_view text, std::size_t width) {
    if (text.size() >= width) return std::string(text);
    return std::string(width - text.size(), ' ') + std::string(text);
}

std::string padRightAscii(std::string_view text, std::size_t width) {
    if (text.size() >= width) return std::string(text);
    return std::string(text) + std::string(width - text.size(), ' ');
}

std::string_view stripIndent(std::string_view line, int& indentColumns, int tabWidth) {
    indentColumns = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line[i] == ' ') {
            ++indentColumns;
        } else if (line[i] == '\t') {
            indentColumns += tabWidth - (indentColumns % tabWidth);
        } else {
            break;
        }
        ++i;
    }
    return line.substr(i);
}

}  // namespace mdview::str
