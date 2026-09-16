#include "search/search.hpp"

#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <algorithm>

namespace mdview::search {

std::string toHaystack(std::string_view text, bool caseSensitive) {
    if (caseSensitive) return std::string(text);
    // Lowercasing ASCII only: byte lengths are preserved, which is what lets
    // offsets found in the haystack index into the original text.  Non-ASCII
    // bytes pass through untouched, so matching stays byte-exact there.
    return mdview::str::toLowerAscii(text);
}

std::vector<std::size_t> findMatchesInLine(std::string_view lineText, std::string_view query,
                                           bool caseSensitive) {
    std::vector<std::size_t> offsets;
    if (query.empty() || lineText.size() < query.size()) return offsets;

    const std::string haystack = toHaystack(lineText, caseSensitive);
    const std::string needle = toHaystack(query, caseSensitive);

    for (std::size_t at = haystack.find(needle); at != std::string_view::npos;
         at = haystack.find(needle, at + needle.size())) {
        offsets.push_back(at);
    }
    return offsets;
}

std::vector<int> columnOffsets(std::string_view text) {
    std::vector<int> offsets;
    offsets.reserve(text.size() + 1);
    std::size_t i = 0;
    int column = 0;
    while (i < text.size()) {
        offsets.push_back(column);
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(text, i, codePoint);
        if (consumed == 0) break;
        column += uni::codepointWidth(codePoint);
        i += consumed;
    }
    offsets.push_back(column);
    return offsets;
}

SearchByteRange byteRangeForColumns(std::string_view text, int columnStart, int columnEnd) {
    const std::vector<int> offsets = columnOffsets(text);
    const int last = static_cast<int>(offsets.size()) - 1;

    const int beginColumn = std::clamp(columnStart, 0, last > 0 ? last : 0);
    const int endColumn = std::clamp(columnEnd, beginColumn, last > 0 ? last : 0);

    SearchByteRange range;
    range.start = static_cast<std::size_t>(offsets[static_cast<std::size_t>(beginColumn)]);
    range.end = static_cast<std::size_t>(offsets[static_cast<std::size_t>(endColumn)]);
    return range;
}

std::size_t nextMatch(const SearchState& state) {
    if (state.matches.empty()) return static_cast<std::size_t>(-1);
    if (state.current == static_cast<std::size_t>(-1) || state.current + 1 >= state.matches.size()) {
        return 0;
    }
    return state.current + 1;
}

std::size_t previousMatch(const SearchState& state) {
    if (state.matches.empty()) return static_cast<std::size_t>(-1);
    if (state.current == static_cast<std::size_t>(-1) || state.current == 0) {
        return state.matches.size() - 1;
    }
    return state.current - 1;
}

bool seekFrom(SearchState& state, std::size_t fromLine, int direction) {
    if (state.matches.empty()) return false;

    const bool forward = direction >= 0;

    // With nothing selected yet, prefer the first match from the cursor line
    // in the chosen direction; only wrap when that side has none.
    if (state.current == static_cast<std::size_t>(-1)) {
        if (forward) {
            const auto it = std::lower_bound(state.matches.begin(), state.matches.end(), fromLine,
                                             [](const SearchMatch& match, std::size_t line) {
                                                 return match.line < line;
                                             });
            if (it != state.matches.end()) {
                state.current = static_cast<std::size_t>(it - state.matches.begin());
                return true;
            }
            state.current = 0;
            return true;
        }

        const auto rit = std::upper_bound(state.matches.begin(), state.matches.end(), fromLine,
                                          [](std::size_t line, const SearchMatch& match) {
                                              return line < match.line;
                                          });
        if (rit != state.matches.begin()) {
            state.current = static_cast<std::size_t>((rit - 1) - state.matches.begin());
            return true;
        }
        state.current = state.matches.size() - 1;
        return true;
    }

    state.current = forward ? nextMatch(state) : previousMatch(state);
    return true;
}

bool seekToFirstAtOrAfter(SearchState& state, std::size_t fromLine) {
    if (state.matches.empty()) return false;
    const auto it = std::lower_bound(state.matches.begin(), state.matches.end(), fromLine,
                                     [](const SearchMatch& match, std::size_t line) {
                                         return match.line < line;
                                     });
    if (it != state.matches.end()) {
        state.current = static_cast<std::size_t>(it - state.matches.begin());
        return true;
    }
    state.current = 0;
    return true;
}

}  // namespace mdview::search
