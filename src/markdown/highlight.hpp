#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

// A small, dependency-free syntax highlighter.
//
// It is not a parser: it classifies comments, strings, numbers, keywords and
// call-like identifiers with a single pass over the text.  That is more than
// enough to make a fenced code block readable in a terminal, and it keeps the
// project free of a large third-party dependency that would also have to build
// cleanly with MSVC, GCC and Clang.

namespace mdview::markdown {

enum class CodeTokenKind {
    Plain,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Function,
    Operator,
    Punctuation,
    Preprocessor,
    Constant,
};

struct CodeToken {
    CodeTokenKind kind = CodeTokenKind::Plain;
    std::size_t begin = 0;  ///< byte offset into the code text
    std::size_t end = 0;    ///< exclusive
};

/// Classifies `code`.  `language` is the fenced code info string ("cpp",
/// "python", ...).  Unknown languages get a generic C-like pass, which still
/// finds comments, strings and numbers.
///
/// The returned tokens tile the whole input in order.
std::vector<CodeToken> highlightCode(std::string_view code, std::string_view language);

/// True when the language has a dedicated rule set; used by tests and docs.
bool isKnownLanguage(std::string_view language);

}  // namespace mdview::markdown
