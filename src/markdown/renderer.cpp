#include "markdown/renderer.hpp"

#include "markdown/highlight.hpp"
#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace mdview::markdown {

int RenderedLine::displayWidth() const {
    int width = 0;
    for (const Span& span : spans) width += uni::displayWidth(span.text);
    return width;
}

namespace {

using terminal::Style;

/// Effectively "no wrapping": wide enough for any realistic line.
constexpr int kUnwrappedLimit = 1 << 20;

struct Glyph {
    char32_t codePoint = U' ';
    Style style;
};

struct Word {
    std::vector<Glyph> glyphs;
    int width = 0;
    bool forcedBreak = false;
};

void appendString(std::string_view text, const Style& style, std::vector<Glyph>& out) {
    std::size_t i = 0;
    while (i < text.size()) {
        char32_t codePoint = 0;
        const std::size_t consumed = uni::decode(text, i, codePoint);
        if (consumed == 0) break;
        i += consumed;
        if (uni::codepointWidth(codePoint) <= 0) continue;
        out.push_back(Glyph{codePoint, style});
    }
}

void appendGlyphs(std::vector<Span>& spans, const std::vector<Glyph>& glyphs, std::size_t begin,
                  std::size_t end) {
    for (std::size_t i = begin; i < end && i < glyphs.size(); ++i) {
        const Glyph& glyph = glyphs[i];
        if (!spans.empty() && spans.back().style == glyph.style) {
            uni::appendUtf8(glyph.codePoint, spans.back().text);
        } else {
            Span span;
            span.style = glyph.style;
            uni::appendUtf8(glyph.codePoint, span.text);
            spans.push_back(std::move(span));
        }
    }
}

int glyphsWidth(const std::vector<Glyph>& glyphs) {
    int width = 0;
    for (const Glyph& glyph : glyphs) width += uni::codepointWidth(glyph.codePoint);
    return width;
}

/// Keeps at most `maxWidth` columns of `glyphs`.
void clipGlyphs(std::vector<Glyph>& glyphs, int maxWidth) {
    int width = 0;
    std::size_t keep = 0;
    while (keep < glyphs.size()) {
        const int glyphWidth = uni::codepointWidth(glyphs[keep].codePoint);
        if (width + glyphWidth > maxWidth) break;
        width += glyphWidth;
        ++keep;
    }
    glyphs.resize(keep);
}

std::vector<Word> splitWords(const std::vector<Glyph>& glyphs) {
    std::vector<Word> words;
    Word current;
    for (const Glyph& glyph : glyphs) {
        if (glyph.codePoint == U'\n') {
            if (!current.glyphs.empty()) {
                words.push_back(std::move(current));
                current = Word{};
            }
            Word br;
            br.forcedBreak = true;
            words.push_back(std::move(br));
            continue;
        }
        if (uni::isBreakableSpace(glyph.codePoint)) {
            if (!current.glyphs.empty()) {
                words.push_back(std::move(current));
                current = Word{};
            }
            continue;
        }
        current.width += uni::codepointWidth(glyph.codePoint);
        current.glyphs.push_back(glyph);
    }
    if (!current.glyphs.empty()) words.push_back(std::move(current));
    return words;
}

class Renderer {
public:
    explicit Renderer(const RenderOptions& options)
        : options_(options), theme_(options.theme ? *options.theme : fallbackTheme()) {
        width_ = std::max(20, options.width);
        text_ = theme_.text;
    }

    RenderedDocument run(const Document& document) {
        for (const Block& block : document.blocks) {
            renderBlock(block, 0, 0, false);
        }
        while (!lines_.empty() && lines_.back().spans.empty()) lines_.pop_back();

        RenderedDocument result;
        result.lines = std::move(lines_);
        result.headingLines = std::move(headings_);
        return result;
    }

private:
    static const terminal::Theme& fallbackTheme() {
        static const terminal::Theme theme = terminal::Theme::plain();
        return theme;
    }

    // --- line emission ----------------------------------------------------

    void pushLine(std::vector<Span> spans, RenderedLine::Kind kind, int headingLevel = 0) {
        RenderedLine line;
        line.spans = std::move(spans);
        line.kind = kind;
        line.headingLevel = headingLevel;
        if (headingLevel > 0) headings_.push_back(lines_.size());
        lines_.push_back(std::move(line));
    }

    void ensureBlank() {
        if (lines_.empty()) return;
        if (lines_.back().spans.empty()) return;
        lines_.push_back(RenderedLine{});
    }

    // --- inline flattening ------------------------------------------------

    void appendInlines(const std::vector<Inline>& inlines, const Style& base,
                       std::vector<Glyph>& out) const {
        for (const Inline& node : inlines) {
            switch (node.type) {
                case InlineType::Text:
                    appendString(node.text, base, out);
                    break;
                case InlineType::Code: {
                    Style style = theme_.inlineCode;
                    appendString(node.text, style, out);
                    break;
                }
                case InlineType::Emphasis: {
                    Style style = base;
                    style.italic = true;
                    appendInlines(node.children, style, out);
                    break;
                }
                case InlineType::Strong: {
                    Style style = base;
                    style.bold = true;
                    appendInlines(node.children, style, out);
                    break;
                }
                case InlineType::Strikethrough: {
                    Style style = base;
                    style.strikethrough = true;
                    appendInlines(node.children, style, out);
                    break;
                }
                case InlineType::Link: {
                    Style style = base;
                    style.fg = theme_.link.fg;
                    style.underline = true;
                    appendInlines(node.children, style, out);
                    const std::string label = plainText(node.children);
                    if (!node.href.empty() && node.href != label) {
                        Style url = theme_.linkUrl;
                        appendString(" (", url, out);
                        appendString(node.href, url, out);
                        appendString(")", url, out);
                    }
                    break;
                }
                case InlineType::Image: {
                    appendString(options_.unicode ? "\xF0\x9F\x96\xBC " : "[image] ", theme_.image, out);
                    appendInlines(node.children, theme_.image, out);
                    break;
                }
                case InlineType::LineBreak:
                    out.push_back(Glyph{U'\n', base});
                    break;
                case InlineType::SoftBreak:
                    out.push_back(Glyph{U' ', base});
                    break;
            }
        }
    }

    // --- wrapping ---------------------------------------------------------

    void addWrapped(const std::vector<Glyph>& glyphs, int firstIndent, int continuationIndent,
                    std::vector<Span> firstPrefix, RenderedLine::Kind kind, int headingLevel) {
        // With wrapping off, the logical line is emitted whole and the view
        // scrolls horizontally instead.
        const int limit = options_.wrap ? width_ : kUnwrappedLimit;
        // Guarantee at least one usable column so wrapping always progresses.
        firstIndent = std::clamp(firstIndent, 0, std::max(0, limit - 2));
        continuationIndent = std::clamp(continuationIndent, 0, std::max(0, limit - 2));

        const std::vector<Word> words = splitWords(glyphs);

        std::vector<Span> line;
        int lineWidth = 0;
        int lineIndent = firstIndent;
        bool emittedAny = false;

        const auto beginFirst = [&]() {
            line = std::move(firstPrefix);
            firstPrefix.clear();
            lineWidth = firstIndent;
            lineIndent = firstIndent;
        };
        const auto beginContinuation = [&]() {
            line.clear();
            if (continuationIndent > 0) {
                line.push_back(Span{std::string(static_cast<std::size_t>(continuationIndent), ' '), text_});
            }
            lineWidth = continuationIndent;
            lineIndent = continuationIndent;
        };
        const auto endLine = [&]() {
            pushLine(std::move(line), kind, emittedAny ? 0 : headingLevel);
            emittedAny = true;
        };

        beginFirst();
        for (const Word& word : words) {
            if (word.forcedBreak) {
                endLine();
                beginContinuation();
                continue;
            }

            // Prefer moving a word that does not fit to the next line over
            // splitting it; only break inside a word when it is wider than an
            // entire line all by itself.
            const bool lineHasContent = lineWidth > lineIndent;
            const int spaceWidth = lineHasContent ? 1 : 0;
            const int availableHere = limit - lineWidth - spaceWidth;
            const int availableFresh = limit - continuationIndent;
            if (lineHasContent && word.width > availableHere && word.width <= availableFresh) {
                endLine();
                beginContinuation();
            }

            std::size_t offset = 0;
            while (offset < word.glyphs.size()) {
                const bool needsSpace = lineWidth > lineIndent;
                const int lead = needsSpace ? 1 : 0;
                int available = limit - lineWidth - lead;
                if (available < 1) {
                    if (lineWidth > lineIndent) {
                        endLine();
                        beginContinuation();
                        continue;
                    }
                    available = 1;  // last resort: always make progress
                }

                std::size_t end = offset;
                int chunkWidth = 0;
                while (end < word.glyphs.size()) {
                    const int glyphWidth = uni::codepointWidth(word.glyphs[end].codePoint);
                    if (chunkWidth > 0 && chunkWidth + glyphWidth > available) break;
                    chunkWidth += glyphWidth;
                    ++end;
                    if (chunkWidth >= available) break;
                }
                if (end == offset) {
                    end = offset + 1;
                    chunkWidth = uni::codepointWidth(word.glyphs[offset].codePoint);
                }

                if (lead > 0) {
                    line.push_back(Span{" ", text_});
                    lineWidth += 1;
                }
                appendGlyphs(line, word.glyphs, offset, end);
                lineWidth += chunkWidth;
                offset = end;

                if (offset < word.glyphs.size()) {
                    endLine();
                    beginContinuation();
                }
            }
        }

        // Emit the final line.  When the content ended exactly on a line
        // boundary there is nothing left to push.
        if (lineWidth > lineIndent || !emittedAny) {
            pushLine(std::move(line), kind, emittedAny ? 0 : headingLevel);
        }
    }

    // --- blocks -----------------------------------------------------------

    void renderBlock(const Block& block, int indent, int depth, bool tight) {
        switch (block.type) {
            case BlockType::Heading:
                renderHeading(block, indent);
                break;
            case BlockType::Paragraph:
                renderParagraph(block, indent, tight);
                break;
            case BlockType::CodeBlock:
                renderCodeBlock(block, indent, tight);
                break;
            case BlockType::List:
                renderList(block, indent, depth, tight);
                break;
            case BlockType::ListItem:
                for (const Block& child : block.children) renderBlock(child, indent, depth + 1, tight);
                break;
            case BlockType::BlockQuote:
                renderBlockQuote(block, indent, depth, tight);
                break;
            case BlockType::ThematicBreak:
                if (!tight) ensureBlank();
                renderRule(indent);
                ensureBlank();
                break;
            case BlockType::Table:
                renderTable(block, indent, tight);
                break;
        }
    }

    void renderHeading(const Block& block, int indent) {
        ensureBlank();
        const int level = std::clamp(block.level, 1, 6);
        std::vector<Glyph> glyphs;
        appendInlines(block.inlines, theme_.heading[static_cast<std::size_t>(level - 1)], glyphs);
        const int textWidth = glyphsWidth(glyphs);

        addWrapped(glyphs, indent, indent, {}, RenderedLine::Kind::Text, level);

        if (options_.unicode && level <= 2 && width_ - indent >= 4) {
            const std::string glyph = level == 1 ? "\u2550" : "\u2500";
            const int length = std::clamp(textWidth, 4, std::max(4, width_ - indent));
            pushLine({Span{std::string(static_cast<std::size_t>(indent), ' ') +
                               str::repeat(glyph, static_cast<std::size_t>(length)),
                           theme_.rule}},
                     RenderedLine::Kind::Rule);
        }
        ensureBlank();
    }

    void renderParagraph(const Block& block, int indent, bool tight) {
        std::vector<Glyph> glyphs;
        appendInlines(block.inlines, text_, glyphs);
        if (glyphs.empty()) return;
        // `tight` means "this paragraph is a list item's text": no surrounding
        // blank lines, so consecutive items read as one list.
        if (!tight) ensureBlank();
        addWrapped(glyphs, indent, indent, {}, RenderedLine::Kind::Text, 0);
        if (!tight) ensureBlank();
    }

    void renderRule(int indent) {
        const int length = std::max(3, width_ - indent);
        const std::string glyph = options_.unicode ? "\u2500" : "-";
        pushLine({Span{std::string(static_cast<std::size_t>(indent), ' ') +
                           str::repeat(glyph, static_cast<std::size_t>(length)),
                       theme_.rule}},
                 RenderedLine::Kind::Rule);
    }

    void renderBlockQuote(const Block& block, int indent, int depth, bool tight) {
        if (!tight) ensureBlank();

        const Style savedText = text_;
        text_ = theme_.quoteText;

        const std::size_t begin = lines_.size();
        const int innerIndent = indent + 2;
        for (const Block& child : block.children) {
            renderBlock(child, innerIndent, depth + 1, false);
        }
        text_ = savedText;

        const Span bar{options_.unicode ? "\u2502 " : "| ", theme_.quoteBar};
        for (std::size_t i = begin; i < lines_.size(); ++i) {
            lines_[i].spans.insert(lines_[i].spans.begin(), bar);
        }
        if (!tight) ensureBlank();
    }

    Style tokenStyle(CodeTokenKind kind) const {
        switch (kind) {
            case CodeTokenKind::Keyword:
                return theme_.code.keyword;
            case CodeTokenKind::Type:
                return theme_.code.type;
            case CodeTokenKind::String:
                return theme_.code.string;
            case CodeTokenKind::Number:
                return theme_.code.number;
            case CodeTokenKind::Comment:
                return theme_.code.comment;
            case CodeTokenKind::Function:
                return theme_.code.function;
            case CodeTokenKind::Operator:
                return theme_.code.operatorSymbol;
            case CodeTokenKind::Punctuation:
                return theme_.code.punctuation;
            case CodeTokenKind::Preprocessor:
                return theme_.code.preprocessor;
            case CodeTokenKind::Constant:
                return theme_.code.constant;
            case CodeTokenKind::Plain:
                break;
        }
        return theme_.code.plain;
    }

    void renderCodeBlock(const Block& block, int indent, bool tight) {
        if (!tight) ensureBlank();

        const int boxWidth = width_ - indent;
        const std::string pad(static_cast<std::size_t>(std::max(0, indent)), ' ');
        const std::vector<std::string> codeLines = str::splitLines(block.literal);

        const Style frame = theme_.codeBorder;
        const Style body = theme_.codeBlock;

        // Box glyphs degrade to ASCII on terminals that cannot be trusted with
        // the Unicode set.
        const bool unicode = options_.unicode;
        const std::string vertical = unicode ? "\u2502" : "|";
        const std::string horizontal = unicode ? "\u2500" : "-";
        const std::string topLeft = unicode ? "\u250C" : "+";
        const std::string topRight = unicode ? "\u2510" : "+";
        const std::string bottomLeft = unicode ? "\u2514" : "+";
        const std::string bottomRight = unicode ? "\u2518" : "+";

        // In a very narrow pane a frame costs more than it gives, so fall back
        // to a simple indented block.
        if (boxWidth < 14) {
            for (const std::string& sourceLine : codeLines) {
                std::vector<Glyph> glyphs;
                appendString(sourceLine, body, glyphs);
                std::size_t offset = 0;
                if (glyphs.empty()) {
                    pushLine({Span{pad + "  ", body}}, RenderedLine::Kind::Code);
                    continue;
                }
                while (offset < glyphs.size()) {
                    const int available = std::max(1, width_ - indent - 2);
                    std::size_t end = offset;
                    int used = 0;
                    while (end < glyphs.size()) {
                        const int glyphWidth = uni::codepointWidth(glyphs[end].codePoint);
                        if (used > 0 && used + glyphWidth > available) break;
                        used += glyphWidth;
                        ++end;
                        if (used >= available) break;
                    }
                    std::vector<Span> row{Span{pad + "  ", body}};
                    appendGlyphs(row, glyphs, offset, end);
                    pushLine(std::move(row), RenderedLine::Kind::Code);
                    offset = end;
                }
            }
            if (!tight) ensureBlank();
            return;
        }

        const int inner = boxWidth - 4;  // "| " + inner + " |"

        // --- top border, with the language label --------------------------
        {
            std::string label;
            if (!block.info.empty()) {
                label = " " + str::toLowerAscii(block.info) + " ";
                if (uni::displayWidth(label) > boxWidth - 4) {
                    label = " " + uni::truncate(block.info, boxWidth - 6, "") + " ";
                }
            }
            const int labelWidth = uni::displayWidth(label);
            const int fill = std::max(0, boxWidth - 3 - labelWidth);
            std::string top = topLeft + horizontal + label +
                              str::repeat(horizontal, static_cast<std::size_t>(fill)) + topRight;
            pushLine({Span{pad + top, frame}}, RenderedLine::Kind::Code);
        }

        // Tokenise once for the whole block so multi-line comments and strings
        // are highlighted correctly.
        const std::vector<CodeToken> tokens =
            options_.syntaxHighlight && !block.literal.empty()
                ? highlightCode(block.literal, block.info)
                : std::vector<CodeToken>{};

        std::size_t lineStart = 0;
        for (const std::string& sourceLine : codeLines) {
            const std::size_t lineEnd = lineStart + sourceLine.size();

            std::vector<Glyph> glyphs;
            if (tokens.empty()) {
                appendString(sourceLine, body, glyphs);
            } else {
                for (const CodeToken& token : tokens) {
                    const std::size_t begin = std::max(token.begin, lineStart);
                    const std::size_t end = std::min(token.end, lineEnd);
                    if (end <= begin) continue;
                    appendString(std::string_view(block.literal).substr(begin, end - begin),
                                 tokenStyle(token.kind), glyphs);
                }
            }

            if (glyphs.empty()) {
                std::string row = pad + vertical + " " +
                                  std::string(static_cast<std::size_t>(inner), ' ') + " " + vertical;
                pushLine({Span{std::move(row), body}}, RenderedLine::Kind::Code);
            } else {
                std::size_t offset = 0;
                while (offset < glyphs.size()) {
                    std::size_t end = offset;
                    int used = 0;
                    while (end < glyphs.size()) {
                        const int glyphWidth = uni::codepointWidth(glyphs[end].codePoint);
                        if (used > 0 && used + glyphWidth > inner) break;
                        used += glyphWidth;
                        ++end;
                        if (used >= inner) break;
                    }
                    if (end == offset) {
                        end = offset + 1;
                        used = uni::codepointWidth(glyphs[offset].codePoint);
                    }
                    std::vector<Span> row;
                    row.push_back(Span{pad + vertical + " ", frame});
                    appendGlyphs(row, glyphs, offset, end);
                    row.push_back(Span{std::string(static_cast<std::size_t>(std::max(0, inner - used)), ' '), body});
                    row.push_back(Span{" " + vertical, frame});
                    pushLine(std::move(row), RenderedLine::Kind::Code);
                    offset = end;
                }
            }
            lineStart = lineEnd + 1;  // skip the '\n'
        }

        pushLine({Span{pad + bottomLeft +
                           str::repeat(horizontal, static_cast<std::size_t>(std::max(0, boxWidth - 2))) +
                           bottomRight,
                       frame}},
                 RenderedLine::Kind::Code);
        if (!tight) ensureBlank();
    }

    std::string bulletFor(int depth) const {
        if (!options_.unicode) return "-";
        switch (depth % 3) {
            case 1:
                return "\u25E6";  // ◦
            case 2:
                return "\u25AA";  // ▪
            default:
                return "\u2022";  // •
        }
    }

    void renderList(const Block& list, int indent, int depth, bool tight) {
        if (!tight) ensureBlank();

        for (std::size_t index = 0; index < list.children.size(); ++index) {
            const Block& item = list.children[index];

            std::string markerText;
            Style markerStyle = theme_.bullet;
            if (item.task) {
                if (item.checked) {
                    markerText = options_.unicode ? "\u2611 " : "[x] ";
                    markerStyle = theme_.checkboxChecked;
                } else {
                    markerText = options_.unicode ? "\u2610 " : "[ ] ";
                    markerStyle = theme_.checkbox;
                }
            } else if (list.ordered) {
                markerText = std::to_string(list.start + static_cast<int>(index)) + ". ";
                markerStyle = theme_.listMarker;
            } else {
                markerText = bulletFor(depth) + " ";
            }

            const int markerWidth = uni::displayWidth(markerText);
            const int contentIndent = indent + markerWidth;

            const std::size_t firstLine = lines_.size();
            for (std::size_t child = 0; child < item.children.size(); ++child) {
                // A nested list or block should hug the item text rather than
                // being separated by a blank line.
                const bool childTight = child == 0 ? true : (item.children[child].type == BlockType::List);
                renderBlock(item.children[child], contentIndent, depth + 1, childTight);
            }

            if (lines_.size() == firstLine) {
                pushLine({Span{std::string(static_cast<std::size_t>(indent), ' ') + markerText, markerStyle}},
                         RenderedLine::Kind::Text);
                continue;
            }

            RenderedLine& line = lines_[firstLine];
            if (line.spans.empty()) {
                RenderedLine markerLine;
                markerLine.spans.push_back(
                    Span{std::string(static_cast<std::size_t>(indent), ' ') + markerText, markerStyle});
                lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(firstLine),
                              std::move(markerLine));
                continue;
            }

            // Replace the leading indent span so the marker sits at `indent`.
            if (!line.spans.empty()) {
                const std::string& leading = line.spans.front().text;
                if (!leading.empty() && leading.find_first_not_of(' ') == std::string::npos) {
                    line.spans.erase(line.spans.begin());
                }
            }
            line.spans.insert(line.spans.begin(), Span{markerText, markerStyle});
            if (indent > 0) {
                line.spans.insert(line.spans.begin(),
                                  Span{std::string(static_cast<std::size_t>(indent), ' '), text_});
            }
        }

        if (!tight) ensureBlank();
    }

    void renderTable(const Block& table, int indent, bool tight) {
        if (!tight) ensureBlank();

        const std::size_t columns = table.alignments.size();
        if (columns == 0 || table.rows.empty()) return;

        // Natural width of every column.
        std::vector<int> widths(columns, 3);
        std::vector<std::vector<std::string>> cells(table.rows.size());
        for (std::size_t row = 0; row < table.rows.size(); ++row) {
            cells[row].resize(columns);
            for (std::size_t column = 0; column < columns; ++column) {
                const TableCell& cell = table.rows[row][column];
                cells[row][column] = plainText(cell.inlines);
                widths[column] = std::max(widths[column], uni::displayWidth(cells[row][column]));
            }
        }

        const int available = std::max(20, width_ - indent);
        const int overhead = 3 * static_cast<int>(columns) + 1;  // padding + borders
        int budget = std::max(static_cast<int>(columns) * 3, available - overhead);
        int total = std::accumulate(widths.begin(), widths.end(), 0);
        while (total > budget) {
            std::size_t widest = 0;
            for (std::size_t column = 1; column < columns; ++column) {
                if (widths[column] > widths[widest]) widest = column;
            }
            if (widths[widest] <= 3) break;
            --widths[widest];
            --total;
        }

        const std::string pad(static_cast<std::size_t>(std::max(0, indent)), ' ');
        const bool box = options_.unicode;
        const std::string horizontal = box ? "\u2500" : "-";
        const std::string vertical = box ? "\u2502" : "|";
        const std::string topLeft = box ? "\u250C" : "+";
        const std::string topJoin = box ? "\u252C" : "+";
        const std::string topRight = box ? "\u2510" : "+";
        const std::string midLeft = box ? "\u251C" : "+";
        const std::string midJoin = box ? "\u253C" : "+";
        const std::string midRight = box ? "\u2524" : "+";
        const std::string bottomLeft = box ? "\u2514" : "+";
        const std::string bottomJoin = box ? "\u2534" : "+";
        const std::string bottomRight = box ? "\u2518" : "+";

        const auto buildBorder = [&](const std::string& left, const std::string& join,
                                     const std::string& right) {
            std::string text = left;
            for (std::size_t column = 0; column < columns; ++column) {
                text += str::repeat(horizontal, static_cast<std::size_t>(widths[column] + 2));
                text += (column + 1 == columns) ? right : join;
            }
            return text;
        };

        pushLine({Span{pad + buildBorder(topLeft, topJoin, topRight), theme_.tableBorder}},
                 RenderedLine::Kind::Table);

        for (std::size_t row = 0; row < table.rows.size(); ++row) {
            const bool header = row == 0;
            const Style contentStyle = header ? theme_.tableHeader : text_;

            std::vector<Span> line;
            line.push_back(Span{pad + vertical, theme_.tableBorder});
            for (std::size_t column = 0; column < columns; ++column) {
                const TableCell& cell = table.rows[row][column];
                std::vector<Glyph> glyphs;
                appendInlines(cell.inlines, contentStyle, glyphs);
                clipGlyphs(glyphs, widths[column]);

                const int contentWidth = glyphsWidth(glyphs);
                const int spare = std::max(0, widths[column] - contentWidth);
                int leftPad = 0;
                switch (cell.align) {
                    case Alignment::Right:
                        leftPad = spare;
                        break;
                    case Alignment::Center:
                        leftPad = spare / 2;
                        break;
                    case Alignment::Left:
                    case Alignment::None:
                        break;
                }
                const int rightPad = spare - leftPad;

                line.push_back(Span{" ", theme_.tableBorder});
                if (leftPad > 0) {
                    line.push_back(Span{std::string(static_cast<std::size_t>(leftPad), ' '), contentStyle});
                }
                appendGlyphs(line, glyphs, 0, glyphs.size());
                if (rightPad > 0) {
                    line.push_back(Span{std::string(static_cast<std::size_t>(rightPad), ' '), contentStyle});
                }
                line.push_back(Span{" ", theme_.tableBorder});
                line.push_back(Span{vertical, theme_.tableBorder});
            }
            pushLine(std::move(line), RenderedLine::Kind::Table);

            if (header && table.rows.size() > 1) {
                pushLine({Span{pad + buildBorder(midLeft, midJoin, midRight), theme_.tableBorder}},
                         RenderedLine::Kind::Table);
            }
        }

        pushLine({Span{pad + buildBorder(bottomLeft, bottomJoin, bottomRight), theme_.tableBorder}},
                 RenderedLine::Kind::Table);
        if (!tight) ensureBlank();
    }

    RenderOptions options_;
    terminal::Theme theme_;
    Style text_;
    int width_ = 80;
    std::vector<RenderedLine> lines_;
    std::vector<std::size_t> headings_;
};

}  // namespace

std::string plainText(const std::vector<Inline>& inlines) {
    std::string out;
    for (const Inline& node : inlines) {
        switch (node.type) {
            case InlineType::Text:
            case InlineType::Code:
                out += node.text;
                break;
            case InlineType::Emphasis:
            case InlineType::Strong:
            case InlineType::Strikethrough:
            case InlineType::Link:
            case InlineType::Image:
                out += plainText(node.children);
                break;
            case InlineType::LineBreak:
            case InlineType::SoftBreak:
                out.push_back(' ');
                break;
        }
    }
    return out;
}

RenderedDocument renderMarkdown(const Document& document, const RenderOptions& options) {
    Renderer renderer(options);
    return renderer.run(document);
}

}  // namespace mdview::markdown
