#include "test_framework.hpp"

#include "search/search.hpp"

#include <string>
#include <vector>

using namespace mdview;
using namespace mdview::search;

// ---------------------------------------------------------------------------
// Matching
// ---------------------------------------------------------------------------

MDVIEW_TEST(search, finds_all_occurrences) {
    const auto hits = findMatchesInLine("ab cd ab", "ab", false);
    REQUIRE(hits.size() == 2);
    CHECK_EQ(hits[0], std::size_t{0});
    CHECK_EQ(hits[1], std::size_t{6});
}

MDVIEW_TEST(search, ascii_case_folding) {
    const auto folded = findMatchesInLine("Alpha ALPHA alpha", "alpha", false);
    CHECK_EQ(folded.size(), std::size_t{3});

    const auto exact = findMatchesInLine("Alpha ALPHA alpha", "alpha", true);
    CHECK_EQ(exact.size(), std::size_t{1});
}

MDVIEW_TEST(search, overlapping_occurrences) {
    // "aa" matches at 0 and 1: matching restarts one byte past the start.
    const auto hits = findMatchesInLine("aaa", "aa", false);
    REQUIRE(hits.size() == 2);
    CHECK_EQ(hits[0], std::size_t{0});
    CHECK_EQ(hits[1], std::size_t{1});
}

MDVIEW_TEST(search, multibyte_bytes_are_matched_exactly) {
    // ASCII folding must not fold non-ASCII bytes: "É" (0xC3 0x89) differs
    // from "é" (0xC3 0xA9) even case-insensitively.
    const std::string line = "caf\xC3\xA9 CAF\xC3\x89";
    CHECK(findMatchesInLine(line, "caf\xC3\xA9", false).size() == 1);
    CHECK(findMatchesInLine(line, "caf\xC3\x89", false).size() == 1);
    CHECK(findMatchesInLine(line, "caf", true).empty());
}

MDVIEW_TEST(search, degenerate_inputs) {
    CHECK(findMatchesInLine("abc", "", false).empty());
    CHECK(findMatchesInLine("", "abc", false).empty());
    CHECK(findMatchesInLine("ab", "abc", false).empty());
}

// ---------------------------------------------------------------------------
// Columns <-> bytes
// ---------------------------------------------------------------------------

MDVIEW_TEST(search, column_offsets_count_display_width) {
    // "汉" is 2 columns, "a" is 1, so "b" starts at column 3.
    const std::string text = "\xE6\xBC\xA2" "ab";
    const std::vector<int> offsets = columnOffsets(text);
    REQUIRE(offsets.size() == 4);
    CHECK_EQ(offsets[0], 0);
    CHECK_EQ(offsets[1], 2);
    CHECK_EQ(offsets[2], 3);
    CHECK_EQ(offsets[3], 4);
}

MDVIEW_TEST(search, byte_range_for_columns_handles_wide_glyphs) {
    const std::string text = "\xE6\xBC\xA2" "ab";  // 汉 a b
    // Columns 1..2 cover the second half of the wide glyph plus "a": the byte
    // range must start at "a", never inside the wide glyph's bytes.
    const SearchByteRange range = byteRangeForColumns(text, 1, 2);
    CHECK_EQ(range.start, std::size_t{3});
    CHECK_EQ(range.end, std::size_t{4});
}

MDVIEW_TEST(search, byte_range_clamps_out_of_bounds) {
    const SearchByteRange range = byteRangeForColumns("abc", 2, 99);
    CHECK_EQ(range.start, std::size_t{2});
    CHECK_EQ(range.end, std::size_t{3});

    const SearchByteRange empty = byteRangeForColumns("", 0, 5);
    CHECK_EQ(empty.start, std::size_t{0});
    CHECK_EQ(empty.end, std::size_t{0});
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

namespace {

SearchState stateWithMatches(std::initializer_list<SearchMatch> matches) {
    SearchState state;
    state.query.text = "x";
    for (const SearchMatch& match : matches) state.matches.push_back(match);
    return state;
}

}  // namespace

MDVIEW_TEST(search, next_and_previous_wrap) {
    SearchState state = stateWithMatches({{1, 0, 1}, {3, 0, 1}, {5, 0, 1}});

    state.current = 0;
    CHECK_EQ(nextMatch(state), std::size_t{1});
    state.current = 2;
    CHECK_EQ(nextMatch(state), std::size_t{0});
    state.current = 2;
    CHECK_EQ(previousMatch(state), std::size_t{1});
    state.current = 0;
    CHECK_EQ(previousMatch(state), std::size_t{2});
}

MDVIEW_TEST(search, navigation_without_matches) {
    SearchState empty;
    CHECK_EQ(nextMatch(empty), static_cast<std::size_t>(-1));
    CHECK(!seekFrom(empty, 0, 1));
    CHECK(!seekToFirstAtOrAfter(empty, 0));
}

MDVIEW_TEST(search, first_seek_starts_from_the_cursor_line) {
    SearchState state = stateWithMatches({{1, 0, 1}, {3, 0, 1}, {7, 0, 1}});

    CHECK(seekFrom(state, 2, 1));
    CHECK_EQ(state.current, std::size_t{1});  // first match at or after line 2

    SearchState backward = stateWithMatches({{1, 0, 1}, {3, 0, 1}, {7, 0, 1}});
    CHECK(seekFrom(backward, 5, -1));
    CHECK_EQ(backward.current, std::size_t{1});  // last match before line 5
}

MDVIEW_TEST(search, first_seek_wraps_only_when_needed) {
    SearchState state = stateWithMatches({{2, 0, 1}, {4, 0, 1}});

    CHECK(seekFrom(state, 5, 1));
    CHECK_EQ(state.current, std::size_t{0});  // wrapped to the first match

    SearchState backward = stateWithMatches({{2, 0, 1}, {4, 0, 1}});
    CHECK(seekFrom(backward, 1, -1));
    CHECK_EQ(backward.current, std::size_t{1});  // wrapped to the last match
}

MDVIEW_TEST(search, seek_to_first_at_or_after_wraps) {
    SearchState state = stateWithMatches({{2, 0, 1}, {4, 0, 1}});
    CHECK(seekToFirstAtOrAfter(state, 5));
    CHECK_EQ(state.current, std::size_t{0});

    CHECK(seekToFirstAtOrAfter(state, 3));
    CHECK_EQ(state.current, std::size_t{1});
}
