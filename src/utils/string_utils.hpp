#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Byte-oriented helpers.  Everything here operates on UTF-8 as opaque bytes,
// which is safe for the ASCII searches the parser performs; anything that
// needs real code point semantics lives in utils/unicode.hpp.

namespace mdview::str {

bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);

std::string_view trimLeft(std::string_view text);
std::string_view trimRight(std::string_view text);
std::string_view trim(std::string_view text);

bool isBlank(std::string_view text);

std::string toLowerAscii(std::string_view text);
std::string toUpperAscii(std::string_view text);
bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b);
int compareIgnoreCaseAscii(std::string_view a, std::string_view b);

/// Splits on a single ASCII delimiter.  When `keepEmpty` is false, empty
/// fields are dropped (so "a,,b" yields {"a", "b"}).
std::vector<std::string_view> split(std::string_view text, char delimiter, bool keepEmpty = false);

/// Splits text into lines, accepting LF, CRLF and lone CR terminators.  A
/// trailing newline does not produce a final empty line.
std::vector<std::string> splitLines(std::string_view text);

/// UTF-8 aware split: returns the byte offset just past the next line break,
/// or text.size() when there is none.  `lineEnd` excludes the terminator.
std::size_t findLineBreak(std::string_view text, std::size_t from, std::size_t& lineEnd);

std::string replaceAll(std::string_view text, std::string_view from, std::string_view to);
std::string repeat(std::string_view text, std::size_t count);

/// Left / right pads with ASCII spaces up to a byte count.
std::string padLeftAscii(std::string_view text, std::size_t width);
std::string padRightAscii(std::string_view text, std::size_t width);

/// Trims leading spaces and tabs and returns how many bytes were removed.
std::string_view stripIndent(std::string_view line, int& indentColumns, int tabWidth = 4);

}  // namespace mdview::str
