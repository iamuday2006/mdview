#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// UTF-8 and East-Asian-width helpers.
//
// A terminal cell grid cares about *display columns*, not bytes and not code
// points: "é" is 1 column, "漢" is 2 and a combining accent is 0.  The width
// tables below are a pragmatic subset of Unicode's EastAsianWidth and
// Grapheme_Cluster_Break data -- enough for real-world Markdown without
// dragging in a Unicode library (which would also have to build cleanly with
// MSVC, GCC and Clang).

namespace mdview::uni {

constexpr char32_t kReplacementCharacter = 0xFFFD;

/// Decodes the code point starting at byte `index`.  Returns the number of
/// bytes consumed, which is always at least 1 when `index < text.size()`.
/// Malformed input yields U+FFFD and consumes a single byte, so iteration
/// always makes forward progress.
std::size_t decode(std::string_view text, std::size_t index, char32_t& out);

/// Like decode(), but reports whether the input was well-formed.
bool decodeChecked(std::string_view text, std::size_t index, char32_t& out, std::size_t& consumed);

void appendUtf8(char32_t codePoint, std::string& out);
std::string encodeUtf8(char32_t codePoint);

/// Columns occupied by one code point: 0, 1 or 2.
int codepointWidth(char32_t codePoint);

/// Sum of codepointWidth() over a UTF-8 string.
int displayWidth(std::string_view text);

/// Number of code points in a UTF-8 string.
std::size_t countCodepoints(std::string_view text);

/// Cuts `text` down to at most `maxWidth` columns.  When the text was cut and
/// `ellipsis` is non-empty, the ellipsis is appended and counts towards the
/// budget.
std::string truncate(std::string_view text, int maxWidth, std::string_view ellipsis = "\xE2\x80\xA6");

/// Pads with spaces on the right until the text occupies `width` columns.
std::string padRight(std::string_view text, int width);

/// Returns the byte offset of the code point boundary at or before `column`
/// columns into `text`.
std::size_t byteOffsetForColumn(std::string_view text, int column);

bool isCombining(char32_t codePoint);
bool isWide(char32_t codePoint);
bool isZeroWidth(char32_t codePoint);

/// Whitespace that a line breaker is allowed to wrap on.  U+00A0 is included
/// because it is normally rendered as a space.
bool isBreakableSpace(char32_t codePoint);

}  // namespace mdview::uni
