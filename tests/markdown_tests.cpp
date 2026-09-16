#include "test_framework.hpp"

#include "markdown/highlight.hpp"
#include "markdown/parser.hpp"
#include "markdown/renderer.hpp"
#include "terminal/style.hpp"

#include <string>
#include <vector>

using namespace mdview;
using mdview::markdown::BlockType;
using mdview::markdown::InlineType;

namespace {

const terminal::Theme& plainTheme() {
    static const terminal::Theme theme = terminal::Theme::plain();
    return theme;
}

std::string renderedText(const markdown::RenderedDocument& document) {
    std::string out;
    for (const markdown::RenderedLine& line : document.lines) {
        for (const markdown::Span& span : line.spans) out += span.text;
        out.push_back('\n');
    }
    return out;
}

bool hasInlineType(const std::vector<markdown::Inline>& inlines, InlineType type) {
    for (const markdown::Inline& node : inlines) {
        if (node.type == type) return true;
        if (hasInlineType(node.children, type)) return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Block parsing
// ---------------------------------------------------------------------------

MDVIEW_TEST(parser, atx_headings) {
    const markdown::Document document = markdown::parseMarkdown("# One\n\n### Three\n\n####### seven\n");
    REQUIRE(document.blocks.size() == 3);
    CHECK(document.blocks[0].type == BlockType::Heading);
    CHECK_EQ(document.blocks[0].level, 1);
    CHECK_EQ(markdown::plainText(document.blocks[0].inlines), std::string("One"));
    CHECK_EQ(document.blocks[1].level, 3);
    // Seven hashes is not a heading: it is ordinary paragraph text.
    CHECK(document.blocks[2].type == BlockType::Paragraph);
}

MDVIEW_TEST(parser, atx_heading_strips_closing_hashes) {
    const markdown::Document document = markdown::parseMarkdown("## Title ##\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK_EQ(markdown::plainText(document.blocks[0].inlines), std::string("Title"));
}

MDVIEW_TEST(parser, setext_headings) {
    const markdown::Document document = markdown::parseMarkdown("Title\n=====\n\nSub\n---\n");
    REQUIRE(document.blocks.size() == 2);
    CHECK_EQ(document.blocks[0].level, 1);
    CHECK_EQ(document.blocks[1].level, 2);
}

MDVIEW_TEST(parser, paragraphs_and_thematic_breaks) {
    const markdown::Document document = markdown::parseMarkdown("first line\nsecond line\n\n---\n\nafter\n");
    REQUIRE(document.blocks.size() == 3);
    CHECK(document.blocks[0].type == BlockType::Paragraph);
    CHECK_EQ(markdown::plainText(document.blocks[0].inlines), std::string("first line second line"));
    CHECK(document.blocks[1].type == BlockType::ThematicBreak);
}

MDVIEW_TEST(parser, fenced_code_blocks) {
    const markdown::Document document =
        markdown::parseMarkdown("```cpp\nint x = 1;\n\nreturn x;\n```\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK(document.blocks[0].type == BlockType::CodeBlock);
    CHECK_EQ(document.blocks[0].info, std::string("cpp"));
    CHECK_EQ(document.blocks[0].literal, std::string("int x = 1;\n\nreturn x;"));
}

MDVIEW_TEST(parser, unterminated_fence_runs_to_the_end) {
    const markdown::Document document = markdown::parseMarkdown("```\nno terminator\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK_EQ(document.blocks[0].literal, std::string("no terminator"));
}

MDVIEW_TEST(parser, indented_code_blocks) {
    const markdown::Document document = markdown::parseMarkdown("    indented\n    code\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK(document.blocks[0].type == BlockType::CodeBlock);
    CHECK_EQ(document.blocks[0].literal, std::string("indented\ncode"));
}

MDVIEW_TEST(parser, unordered_lists) {
    const markdown::Document document = markdown::parseMarkdown("- a\n- b\n- c\n");
    REQUIRE(document.blocks.size() == 1);
    const markdown::Block& list = document.blocks[0];
    CHECK(list.type == BlockType::List);
    CHECK(!list.ordered);
    CHECK_EQ(list.children.size(), std::size_t{3});
    CHECK(list.children[0].type == BlockType::ListItem);
    CHECK_EQ(markdown::plainText(list.children[1].children[0].inlines), std::string("b"));
}

MDVIEW_TEST(parser, ordered_lists_keep_their_start) {
    const markdown::Document document = markdown::parseMarkdown("3. a\n4. b\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK(document.blocks[0].ordered);
    CHECK_EQ(document.blocks[0].start, 3);
    CHECK_EQ(document.blocks[0].children.size(), std::size_t{2});
}

MDVIEW_TEST(parser, task_list_items) {
    const markdown::Document document = markdown::parseMarkdown("- [x] done\n- [ ] todo\n");
    REQUIRE(document.blocks.size() == 1);
    const markdown::Block& list = document.blocks[0];
    REQUIRE(list.children.size() == 2);
    CHECK(list.children[0].task);
    CHECK(list.children[0].checked);
    CHECK(list.children[1].task);
    CHECK(!list.children[1].checked);
    CHECK_EQ(markdown::plainText(list.children[0].children[0].inlines), std::string("done"));
}

MDVIEW_TEST(parser, nested_lists) {
    const markdown::Document document = markdown::parseMarkdown("- outer\n  - inner\n");
    REQUIRE(document.blocks.size() == 1);
    const markdown::Block& list = document.blocks[0];
    REQUIRE(list.children.size() == 1);
    const markdown::Block& item = list.children[0];
    REQUIRE(item.children.size() == 2);
    CHECK(item.children[1].type == BlockType::List);
    CHECK_EQ(item.children[1].children.size(), std::size_t{1});
}

MDVIEW_TEST(parser, block_quotes) {
    const markdown::Document document = markdown::parseMarkdown("> quoted line\n> more\n");
    REQUIRE(document.blocks.size() == 1);
    CHECK(document.blocks[0].type == BlockType::BlockQuote);
    REQUIRE(document.blocks[0].children.size() == 1);
    CHECK_EQ(markdown::plainText(document.blocks[0].children[0].inlines),
             std::string("quoted line more"));
}

MDVIEW_TEST(parser, gfm_tables) {
    const markdown::Document document =
        markdown::parseMarkdown("| name | qty |\n| :--- | ---: |\n| one | 1 |\n| two | 2 |\n");
    REQUIRE(document.blocks.size() == 1);
    const markdown::Block& table = document.blocks[0];
    CHECK(table.type == BlockType::Table);
    CHECK_EQ(table.rows.size(), std::size_t{3});
    CHECK_EQ(table.alignments.size(), std::size_t{2});
    CHECK(table.alignments[0] == markdown::Alignment::Left);
    CHECK(table.alignments[1] == markdown::Alignment::Right);
    CHECK_EQ(markdown::plainText(table.rows[2][0].inlines), std::string("two"));
}

// ---------------------------------------------------------------------------
// Inline parsing
// ---------------------------------------------------------------------------

MDVIEW_TEST(parser, inline_emphasis_code_and_links) {
    const std::vector<markdown::Inline> inlines = markdown::parseInlines(
        "**bold** and *italic* and `code` and [link](https://example.com)");
    CHECK(hasInlineType(inlines, InlineType::Strong));
    CHECK(hasInlineType(inlines, InlineType::Emphasis));
    CHECK(hasInlineType(inlines, InlineType::Code));
    CHECK(hasInlineType(inlines, InlineType::Link));
    CHECK_EQ(markdown::plainText(inlines),
             std::string("bold and italic and code and link"));
}

MDVIEW_TEST(parser, inline_strikethrough_and_images) {
    const std::vector<markdown::Inline> inlines =
        markdown::parseInlines("~~gone~~ ![alt](pic.png)");
    CHECK(hasInlineType(inlines, InlineType::Strikethrough));
    CHECK(hasInlineType(inlines, InlineType::Image));
    CHECK_EQ(markdown::plainText(inlines), std::string("gone alt"));
}

MDVIEW_TEST(parser, link_target_and_title) {
    const std::vector<markdown::Inline> inlines =
        markdown::parseInlines("[text](https://example.com \"tip\")");
    REQUIRE(inlines.size() == 1);
    CHECK(inlines[0].type == InlineType::Link);
    CHECK_EQ(inlines[0].href, std::string("https://example.com"));
    CHECK_EQ(inlines[0].title, std::string("tip"));
}

MDVIEW_TEST(parser, autolinks) {
    const std::vector<markdown::Inline> inlines = markdown::parseInlines("<https://example.com>");
    REQUIRE(inlines.size() == 1);
    CHECK(inlines[0].type == InlineType::Link);
    CHECK_EQ(inlines[0].href, std::string("https://example.com"));
}

MDVIEW_TEST(parser, underscores_inside_words_are_literal) {
    const std::vector<markdown::Inline> inlines = markdown::parseInlines("snake_case_name");
    CHECK(!hasInlineType(inlines, InlineType::Emphasis));
    CHECK_EQ(markdown::plainText(inlines), std::string("snake_case_name"));
}

MDVIEW_TEST(parser, backslash_escapes) {
    CHECK_EQ(markdown::plainText(markdown::parseInlines("\\*not emphasis\\*")),
             std::string("*not emphasis*"));
    CHECK_EQ(markdown::plainText(markdown::parseInlines("a \\| b")), std::string("a | b"));
}

MDVIEW_TEST(parser, hard_and_soft_breaks) {
    const std::vector<markdown::Inline> hard = markdown::parseInlines("line  \nnext");
    CHECK(hasInlineType(hard, InlineType::LineBreak));

    const std::vector<markdown::Inline> soft = markdown::parseInlines("line\nnext");
    CHECK(hasInlineType(soft, InlineType::SoftBreak));
}

MDVIEW_TEST(parser, margins_do_not_swallow_spaces) {
    // Two spaces and an asterisk must not become emphasis around " 4 ".
    const std::vector<markdown::Inline> inlines = markdown::parseInlines("3 * 4 * 5");
    CHECK(!hasInlineType(inlines, InlineType::Emphasis));
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

MDVIEW_TEST(renderer, wraps_paragraphs_to_the_width) {
    const markdown::Document document =
        markdown::parseMarkdown("one two three four five six seven eight nine ten eleven");
    markdown::RenderOptions options;
    options.width = 20;
    options.theme = &plainTheme();

    const markdown::RenderedDocument rendered = markdown::renderMarkdown(document, options);
    CHECK(rendered.lines.size() > 1);
    for (const markdown::RenderedLine& line : rendered.lines) {
        CHECK(line.displayWidth() <= 20);
    }
    CHECK_CONTAINS(renderedText(rendered), "eleven");
}

MDVIEW_TEST(renderer, records_heading_lines) {
    const markdown::Document document = markdown::parseMarkdown("# One\n\ntext\n\n## Two\n");
    markdown::RenderOptions options;
    options.width = 40;
    options.theme = &plainTheme();

    const markdown::RenderedDocument rendered = markdown::renderMarkdown(document, options);
    CHECK_EQ(rendered.headingLines.size(), std::size_t{2});
    CHECK_EQ(rendered.lines[rendered.headingLines[0]].headingLevel, 1);
    CHECK_EQ(rendered.lines[rendered.headingLines[1]].headingLevel, 2);
}

MDVIEW_TEST(renderer, wraps_long_words_instead_of_overflowing) {
    // A single word far wider than the viewport must still be split up rather
    // than overflow.  (The renderer clamps its width to a sane minimum, which
    // is why the expectation is 20 and not the requested value.)
    const markdown::Document document = markdown::parseMarkdown(std::string(45, 'o'));
    markdown::RenderOptions options;
    options.width = 20;
    options.theme = &plainTheme();

    const markdown::RenderedDocument rendered = markdown::renderMarkdown(document, options);
    REQUIRE(rendered.lines.size() >= 3);
    int total = 0;
    for (const markdown::RenderedLine& line : rendered.lines) {
        CHECK(line.displayWidth() <= 20);
        total += line.displayWidth();
    }
    // No character is lost when a word has to be broken.
    CHECK_EQ(total, 45);
}

MDVIEW_TEST(renderer, code_blocks_are_boxed_and_labelled) {
    const markdown::Document document = markdown::parseMarkdown("```cpp\nint x = 1;\n```\n");
    markdown::RenderOptions options;
    options.width = 32;
    options.theme = &plainTheme();
    options.unicode = true;

    const markdown::RenderedDocument rendered = markdown::renderMarkdown(document, options);
    const std::string text = renderedText(rendered);
    CHECK_CONTAINS(text, "cpp");
    CHECK_CONTAINS(text, "int x = 1;");
    CHECK_CONTAINS(text, "\xE2\x94\x8C");  // U+250C, top-left corner
    CHECK_CONTAINS(text, "\xE2\x94\x90");  // U+2510, top-right corner
    for (const markdown::RenderedLine& line : rendered.lines) {
        CHECK(line.displayWidth() <= 32);
    }
}

MDVIEW_TEST(renderer, ascii_mode_uses_ascii_borders) {
    const markdown::Document document = markdown::parseMarkdown("```\nplain\n```\n");
    markdown::RenderOptions options;
    options.width = 24;
    options.theme = &plainTheme();
    options.unicode = false;
    options.syntaxHighlight = false;

    const std::string text = renderedText(markdown::renderMarkdown(document, options));
    CHECK_CONTAINS(text, "+");
    CHECK_CONTAINS(text, "plain");
}

MDVIEW_TEST(renderer, tables_render_borders_and_alignment) {
    const markdown::Document document =
        markdown::parseMarkdown("| name | qty |\n| --- | ---: |\n| one | 7 |\n");
    markdown::RenderOptions options;
    options.width = 40;
    options.theme = &plainTheme();
    options.unicode = true;

    const std::string text = renderedText(markdown::renderMarkdown(document, options));
    CHECK_CONTAINS(text, "\xE2\x94\x80");  // U+2500 horizontal
    CHECK_CONTAINS(text, "name");
    CHECK_CONTAINS(text, "qty");
    CHECK_CONTAINS(text, "one");
}

MDVIEW_TEST(renderer, lists_and_quotes_are_marked) {
    const markdown::Document document = markdown::parseMarkdown("- item\n\n> quoted\n");
    markdown::RenderOptions options;
    options.width = 30;
    options.theme = &plainTheme();
    options.unicode = true;

    const std::string text = renderedText(markdown::renderMarkdown(document, options));
    CHECK_CONTAINS(text, "\xE2\x80\xA2");  // U+2022 bullet
    CHECK_CONTAINS(text, "\xE2\x94\x82");  // U+2502 quote bar
    CHECK_CONTAINS(text, "item");
    CHECK_CONTAINS(text, "quoted");
}

MDVIEW_TEST(renderer, no_wrap_keeps_one_line) {
    const markdown::Document document =
        markdown::parseMarkdown("alpha beta gamma delta epsilon zeta eta theta");
    markdown::RenderOptions options;
    options.width = 20;
    options.theme = &plainTheme();
    options.wrap = false;

    const markdown::RenderedDocument rendered = markdown::renderMarkdown(document, options);
    REQUIRE(rendered.lines.size() == 1);
    CHECK(rendered.lines[0].displayWidth() > 20);
}

// ---------------------------------------------------------------------------
// Syntax highlighting
// ---------------------------------------------------------------------------

MDVIEW_TEST(highlight, tokens_tile_the_input) {
    const std::string code = "#include <vector>\nint main() { return 0; } // done\n";
    const std::vector<markdown::CodeToken> tokens = markdown::highlightCode(code, "cpp");

    std::size_t expected = 0;
    for (const markdown::CodeToken& token : tokens) {
        CHECK_EQ(token.begin, expected);
        CHECK(token.end > token.begin);
        expected = token.end;
    }
    CHECK_EQ(expected, code.size());
}

MDVIEW_TEST(highlight, finds_cpp_constructs) {
    const std::string code = "#include <vector>\nconst char* s = \"hi\";\n// trailing\n";
    const std::vector<markdown::CodeToken> tokens = markdown::highlightCode(code, "cpp");

    bool preprocessor = false;
    bool keyword = false;
    bool string = false;
    bool comment = false;
    for (const markdown::CodeToken& token : tokens) {
        if (token.kind == markdown::CodeTokenKind::Preprocessor) preprocessor = true;
        if (token.kind == markdown::CodeTokenKind::Keyword) keyword = true;
        if (token.kind == markdown::CodeTokenKind::String) string = true;
        if (token.kind == markdown::CodeTokenKind::Comment) comment = true;
    }
    CHECK(preprocessor);
    CHECK(keyword);
    CHECK(string);
    CHECK(comment);
}

MDVIEW_TEST(highlight, python_triple_quoted_strings) {
    const std::vector<markdown::CodeToken> tokens =
        markdown::highlightCode("x = \"\"\"doc\nstring\"\"\"\n", "python");
    bool string = false;
    for (const markdown::CodeToken& token : tokens) {
        if (token.kind == markdown::CodeTokenKind::String) string = true;
    }
    CHECK(string);
}

MDVIEW_TEST(highlight, unknown_languages_fall_back_to_generic_rules) {
    CHECK(!markdown::isKnownLanguage("nonexistent"));
    CHECK(markdown::isKnownLanguage("cpp"));
    CHECK(markdown::isKnownLanguage("Python"));

    const std::vector<markdown::CodeToken> tokens =
        markdown::highlightCode("// comment\nvalue = 42\n", "nonexistent");
    bool comment = false;
    bool number = false;
    for (const markdown::CodeToken& token : tokens) {
        if (token.kind == markdown::CodeTokenKind::Comment) comment = true;
        if (token.kind == markdown::CodeTokenKind::Number) number = true;
    }
    CHECK(comment);
    CHECK(number);
}
