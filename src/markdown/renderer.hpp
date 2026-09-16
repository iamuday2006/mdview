#pragma once

#include "markdown/document.hpp"
#include "terminal/style.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Turns a Document into finished screen lines.
//
// Everything width-dependent happens here rather than in the widget: the
// renderer knows the available columns, so it wraps paragraphs, boxes code
// blocks and lays out tables.  The MarkdownView then only has to copy spans
// into the screen buffer, which is what keeps the view simple and makes
// re-rendering on resize a single function call.

namespace mdview::markdown {

struct Span {
    std::string text;
    terminal::Style style;
};

struct RenderedLine {
    enum class Kind { Text, Code, Rule, Table };

    std::vector<Span> spans;
    Kind kind = Kind::Text;
    /// 1-6 when this line starts a heading, otherwise 0.
    int headingLevel = 0;

    int displayWidth() const;
    bool blank() const { return spans.empty(); }
};

struct RenderedDocument {
    std::vector<RenderedLine> lines;
    /// Index into `lines` of each heading, in document order.
    std::vector<std::size_t> headingLines;
};

struct RenderOptions {
    int width = 80;
    const terminal::Theme* theme = nullptr;
    bool syntaxHighlight = true;
    bool unicode = true;
    /// When false, logical lines are emitted unwrapped so the view can scroll
    /// horizontally instead.
    bool wrap = true;
};

RenderedDocument renderMarkdown(const Document& document, const RenderOptions& options);

/// Flattened plain text of an inline tree (link labels, image alt text, ...).
std::string plainText(const std::vector<Inline>& inlines);

}  // namespace mdview::markdown
