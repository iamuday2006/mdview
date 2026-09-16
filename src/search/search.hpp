#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Incremental search over the rendered document.
//
// This module is deliberately pure: no terminal, no screen, no application
// state.  It knows two things -- how to find case-insensitive byte ranges
// inside a line, and how to walk those matches forward and backward.  The
// application owns the state (the query, the current match) and the UI turns
// matches into highlight styling, which keeps search replaceable and testable
// without a screen.

namespace mdview::search {

struct SearchMatch {
    std::size_t line = 0;
    /// First display column occupied by the match (half-open range, in
    /// terminal columns, not bytes -- rendered lines may contain wide glyphs).
    int startColumn = 0;
    /// Column just past the last column of the match.
    int endColumn = 0;
};

/// Byte offsets of the match within one line's UTF-8 text.  The highlighter
/// needs these to split a styled span without double-counting wide glyphs.
struct SearchByteRange {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct SearchQuery {
    std::string text;
    bool caseSensitive = false;
    bool empty() const { return text.empty(); }
};

struct SearchState {
    SearchQuery query;
    /// Index into the match list; size_t(-1) means "no current match yet".
    std::size_t current = static_cast<std::size_t>(-1);
    std::vector<SearchMatch> matches;

    bool active() const { return !query.empty(); }
    bool hasMatches() const { return !matches.empty(); }
    std::size_t matchCount() const { return matches.size(); }
};

/// Byte offsets of every occurrence of `query` in `lineText`.  Matching is
/// ASCII-case-insensitive unless `caseSensitive`; multi-byte characters are
/// matched byte-exactly, so "É" will not match "é".
std::vector<std::size_t> findMatchesInLine(std::string_view lineText, std::string_view query,
                                           bool caseSensitive);

/// Builds the lowercased haystack for a case-insensitive search.  Widths in
/// bytes are preserved, so byte offsets found in the haystack index correctly
/// into the original text.
std::string toHaystack(std::string_view text, bool caseSensitive);

/// Prefix sum of display widths; entry i is the column at which the i-th code
/// point starts, and the last entry is the line's total width.
std::vector<int> columnOffsets(std::string_view text);

/// Byte range of columns `columnStart..columnEnd` within the UTF-8 text.
SearchByteRange byteRangeForColumns(std::string_view text, int columnStart, int columnEnd);

/// Next index circularly past `current`, or 0 when nothing matches.
std::size_t nextMatch(const SearchState& state);
/// Previous index circularly before `current`, or 0 when nothing matches.
std::size_t previousMatch(const SearchState& state);

/// Moves `current` to the next (direction > 0) or previous (direction < 0)
/// match, wrapping at the ends.  When nothing is selected yet, the search
/// starts from the cursor line `fromLine` and wraps only if that side has no
/// match at all.  Returns false when there is nothing to select.
bool seekFrom(SearchState& state, std::size_t fromLine, int direction);

/// Selects the first match on a line at or after `fromLine`, wrapping to the
/// first match overall.  Returns false when there are no matches.
bool seekToFirstAtOrAfter(SearchState& state, std::size_t fromLine);

}  // namespace mdview::search
