#pragma once

#include "markdown/document.hpp"

#include <string_view>
#include <vector>

namespace mdview::markdown {

/// Parses a whole document.
Document parseMarkdown(std::string_view text);

/// Parses one paragraph's worth of inline content.  Exposed separately because
/// it is useful on its own (table cells, list item text) and easy to test.
std::vector<Inline> parseInlines(std::string_view text);

}  // namespace mdview::markdown
