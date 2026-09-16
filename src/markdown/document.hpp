#pragma once

#include <cstddef>
#include <string>
#include <vector>

// The document model produced by the parser and consumed by the renderer.
//
// It is intentionally a small, CommonMark-shaped subset: enough structure for
// headings, paragraphs, emphasis, links, lists, quotes, code, rules and GFM
// tables, without pulling in a full specification implementation.

namespace mdview::markdown {

enum class InlineType {
    Text,
    Code,
    Emphasis,
    Strong,
    Strikethrough,
    Link,
    Image,
    LineBreak,  ///< hard break (two trailing spaces, or a trailing backslash)
    SoftBreak,  ///< a source newline inside a paragraph
};

struct Inline {
    InlineType type = InlineType::Text;
    std::string text;              ///< Text and Code
    std::string href;              ///< Link and Image
    std::string title;             ///< Link and Image
    std::vector<Inline> children;  ///< Emphasis, Strong, Strikethrough, Link, Image
};

enum class BlockType {
    Heading,
    Paragraph,
    CodeBlock,
    List,
    ListItem,
    BlockQuote,
    ThematicBreak,
    Table,
};

enum class Alignment { None, Left, Center, Right };

struct TableCell {
    Alignment align = Alignment::None;
    std::vector<Inline> inlines;
};

struct Block {
    BlockType type = BlockType::Paragraph;

    /// Heading and Paragraph content.
    std::vector<Inline> inlines;

    /// CodeBlock source text (with '\n' separators) and its fence info string.
    std::string literal;
    std::string info;

    /// Heading level (1-6).
    int level = 0;

    /// List properties.
    bool ordered = false;
    int start = 1;
    char marker = '-';

    /// Task list properties (ListItem blocks only).
    bool task = false;
    bool checked = false;

    /// Table: `rows[0]` is the header row.
    std::vector<Alignment> alignments;
    std::vector<std::vector<TableCell>> rows;

    /// Nested content: list items, or the blocks inside a block quote.
    std::vector<Block> children;
};

struct Document {
    std::vector<Block> blocks;
};

}  // namespace mdview::markdown
