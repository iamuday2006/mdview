#include "markdown/parser.hpp"

#include "utils/string_utils.hpp"
#include "utils/unicode.hpp"

#include <algorithm>
#include <cctype>

namespace mdview::markdown {

namespace {

constexpr std::size_t kNoPosition = static_cast<std::size_t>(-1);

bool isSpaceByte(char c) { return c == ' ' || c == '\t'; }

bool isWordByte(char c) {
    const unsigned char byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) != 0 || byte == '_';
}

bool isAsciiPunctuation(char c) {
    switch (c) {
        case '!': case '"': case '#': case '$': case '%': case '&': case '\'': case '(':
        case ')': case '*': case '+': case ',': case '-': case '.': case '/': case ':':
        case ';': case '<': case '=': case '>': case '?': case '@': case '[': case '\\':
        case ']': case '^': case '_': case '`': case '{': case '|': case '}': case '~':
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Line classification
// ---------------------------------------------------------------------------

int indentColumns(std::string_view line, int tabWidth = 4) {
    int columns = 0;
    for (const char c : line) {
        if (c == ' ') {
            ++columns;
        } else if (c == '\t') {
            columns += tabWidth - (columns % tabWidth);
        } else {
            break;
        }
    }
    return columns;
}

/// Byte offset of the first non-whitespace character at or after `columns`
/// columns of indentation.
std::size_t byteOffsetForIndent(std::string_view line, int columns, int tabWidth = 4) {
    int current = 0;
    std::size_t i = 0;
    while (i < line.size() && current < columns) {
        if (line[i] == ' ') {
            ++current;
        } else if (line[i] == '\t') {
            current += tabWidth - (current % tabWidth);
        } else {
            break;
        }
        ++i;
    }
    return i;
}

bool isBlank(std::string_view line) { return str::trimLeft(line).empty(); }

/// "---", "***", "___" (three or more, optionally separated by spaces).
bool isThematicBreak(std::string_view line) {
    std::string_view text = str::trim(line);
    if (text.size() < 3) return false;
    const char marker = text.front();
    if (marker != '-' && marker != '*' && marker != '_') return false;
    int count = 0;
    for (const char c : text) {
        if (c == marker) {
            ++count;
        } else if (c != ' ' && c != '\t') {
            return false;
        }
    }
    return count >= 3;
}

/// Returns 1 for "===" and 2 for "---" when the line is a setext underline.
int setextLevel(std::string_view line) {
    const std::string_view text = str::trim(line);
    if (text.empty()) return 0;
    const char marker = text.front();
    if (marker != '=' && marker != '-') return 0;
    for (const char c : text) {
        if (c != marker) return 0;
    }
    return marker == '=' ? 1 : 2;
}

/// Returns the heading level (1-6) for an ATX heading, or 0.
int atxHeadingLevel(std::string_view line, std::string_view* contentOut = nullptr) {
    const int indent = indentColumns(line);
    if (indent > 3) return 0;
    std::string_view text = line.substr(byteOffsetForIndent(line, indent));
    std::size_t hashes = 0;
    while (hashes < text.size() && text[hashes] == '#') ++hashes;
    if (hashes == 0 || hashes > 6) return 0;
    if (hashes < text.size() && text[hashes] != ' ' && text[hashes] != '\t') return 0;

    std::string_view content = str::trim(text.substr(hashes));
    // Closing sequence of '#' characters is not part of the text.
    std::size_t end = content.size();
    while (end > 0 && content[end - 1] == '#') --end;
    if (end != content.size()) {
        content = str::trimRight(content.substr(0, end));
    }
    if (contentOut) *contentOut = content;
    return static_cast<int>(hashes);
}

struct FenceInfo {
    bool isFence = false;
    char marker = '`';
    int length = 0;
    int indent = 0;
    std::string info;
};

FenceInfo scanFence(std::string_view line) {
    FenceInfo fence;
    const int indent = indentColumns(line);
    if (indent > 3) return fence;
    std::string_view text = line.substr(byteOffsetForIndent(line, indent));
    if (text.empty()) return fence;
    const char marker = text.front();
    if (marker != '`' && marker != '~') return fence;

    std::size_t run = 0;
    while (run < text.size() && text[run] == marker) ++run;
    if (run < 3) return fence;

    fence.isFence = true;
    fence.marker = marker;
    fence.length = static_cast<int>(run);
    fence.indent = indent;
    fence.info = std::string(str::trim(text.substr(run)));
    // A backtick fence's info string may not contain backticks.
    if (marker == '`' && fence.info.find('`') != std::string::npos) {
        fence.isFence = false;
    }
    return fence;
}

bool isFenceEnd(std::string_view line, char marker, int length) {
    const int indent = indentColumns(line);
    if (indent > 3) return false;
    std::string_view text = line.substr(byteOffsetForIndent(line, indent));
    std::size_t run = 0;
    while (run < text.size() && text[run] == marker) ++run;
    if (static_cast<int>(run) < length) return false;
    return str::trim(text.substr(run)).empty();
}

struct ListMarker {
    bool valid = false;
    bool ordered = false;
    int start = 1;
    char bullet = '-';
    int indent = 0;
    int contentIndent = 0;
    std::size_t contentOffset = 0;
    bool task = false;
    bool checked = false;
};

ListMarker scanListMarker(std::string_view line, int tabWidth = 4) {
    ListMarker marker;
    const int indent = indentColumns(line, tabWidth);
    if (indent > 3) return marker;

    std::string_view text = line.substr(byteOffsetForIndent(line, indent, tabWidth));
    if (text.empty()) return marker;

    std::size_t markerLength = 0;
    if (text.front() == '-' || text.front() == '+' || text.front() == '*') {
        marker.ordered = false;
        marker.bullet = text.front();
        markerLength = 1;
    } else if (std::isdigit(static_cast<unsigned char>(text.front())) != 0) {
        std::size_t digits = 0;
        int value = 0;
        while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])) != 0) {
            if (digits < 9) value = value * 10 + (text[digits] - '0');
            ++digits;
        }
        if (digits == 0 || digits > 9 || digits >= text.size()) return marker;
        const char delimiter = text[digits];
        if (delimiter != '.' && delimiter != ')') return marker;
        marker.ordered = true;
        marker.start = value;
        marker.bullet = delimiter;
        markerLength = digits + 1;
    } else {
        return marker;
    }

    if (markerLength > text.size()) return marker;
    if (markerLength < text.size() && !isSpaceByte(text[markerLength])) return marker;

    marker.valid = true;
    marker.indent = indent;

    // Content starts at the first non-space after the marker, with a minimum
    // indent of one column so an empty item still has a valid position.
    std::size_t offset = markerLength;
    int spaces = 0;
    while (offset < text.size() && isSpaceByte(text[offset])) {
        ++spaces;
        ++offset;
    }
    if (offset >= text.size()) spaces = 1;  // empty item still needs a position

    const std::size_t markerStart = byteOffsetForIndent(line, indent, tabWidth);
    marker.contentOffset = markerStart + offset;
    marker.contentIndent = indent + static_cast<int>(markerLength) + std::max(1, spaces);

    // Task list syntax: "- [ ] todo" / "- [x] done".
    if (marker.contentOffset + 3 <= line.size() && line[marker.contentOffset] == '[' &&
        line[marker.contentOffset + 2] == ']') {
        const char state = static_cast<char>(
            std::tolower(static_cast<unsigned char>(line[marker.contentOffset + 1])));
        if (state == ' ' || state == 'x') {
            const std::size_t after = marker.contentOffset + 3;
            if (after >= line.size() || isSpaceByte(line[after])) {
                marker.task = true;
                marker.checked = state == 'x';
                std::size_t advance = 3;
                while (after + (advance - 3) < line.size() &&
                       isSpaceByte(line[after + (advance - 3)])) {
                    ++advance;
                }
                marker.contentOffset += advance;
            }
        }
    }
    return marker;
}

/// Splits a GFM table row on unescaped '|'.
bool splitTableRow(std::string_view line, std::vector<std::string>& cells) {
    cells.clear();
    std::string_view text = str::trim(line);
    if (!text.empty() && text.front() == '|') text.remove_prefix(1);
    if (!text.empty() && text.back() == '|') text.remove_suffix(1);

    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == '|') {
                current.push_back('|');
                ++i;
                continue;
            }
            current.push_back(c);
            continue;
        }
        if (c == '|') {
            cells.push_back(std::string(str::trim(current)));
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    cells.push_back(std::string(str::trim(current)));
    return !cells.empty();
}

/// "---", ":---", "---:", ":---:" for each cell.
bool parseTableDelimiter(std::string_view line, std::vector<Alignment>& alignments) {
    if (line.find('-') == std::string_view::npos) return false;

    std::vector<std::string> cells;
    splitTableRow(line, cells);

    alignments.clear();
    for (const std::string& cell : cells) {
        if (cell.empty()) return false;
        std::size_t start = 0;
        std::size_t end = cell.size();
        bool left = false;
        bool right = false;
        if (cell[start] == ':') {
            left = true;
            ++start;
        }
        if (end > start && cell[end - 1] == ':') {
            right = true;
            --end;
        }
        if (end <= start) return false;
        for (std::size_t i = start; i < end; ++i) {
            if (cell[i] != '-') return false;
        }
        if (left && right) {
            alignments.push_back(Alignment::Center);
        } else if (left) {
            alignments.push_back(Alignment::Left);
        } else if (right) {
            alignments.push_back(Alignment::Right);
        } else {
            alignments.push_back(Alignment::None);
        }
    }
    return !alignments.empty();
}

bool stripBlockQuoteMarker(std::string_view line, std::string_view& out) {
    const int indent = indentColumns(line);
    if (indent > 3) return false;
    std::string_view text = line.substr(byteOffsetForIndent(line, indent));
    if (text.empty() || text.front() != '>') return false;
    text.remove_prefix(1);
    if (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    out = text;
    return true;
}

// ---------------------------------------------------------------------------
// Inline parsing
// ---------------------------------------------------------------------------

void flushText(std::string& pending, std::vector<Inline>& out) {
    if (pending.empty()) return;
    Inline in;
    in.type = InlineType::Text;
    in.text = std::move(pending);
    pending.clear();
    out.push_back(std::move(in));
}

std::size_t findClosingDelimiter(std::string_view text, std::size_t from, std::string_view delimiter,
                                 bool underscoreRule) {
    std::size_t position = from;
    while (position < text.size()) {
        position = text.find(delimiter, position);
        if (position == kNoPosition) return kNoPosition;
        if (position == 0) {
            position += delimiter.size();
            continue;
        }
        // A closing run must be right-flanking: not preceded by whitespace.
        if (isSpaceByte(text[position - 1])) {
            position += delimiter.size();
            continue;
        }
        if (underscoreRule) {
            const std::size_t after = position + delimiter.size();
            if (after < text.size() && isWordByte(text[after])) {
                position += delimiter.size();
                continue;
            }
        }
        return position;
    }
    return kNoPosition;
}

bool looksLikeAutolink(std::string_view inner) {
    if (inner.empty() || inner.find(' ') != std::string_view::npos) return false;
    if (inner.find('<') != std::string_view::npos) return false;
    if (inner.find("://") != std::string_view::npos) return true;
    if (str::startsWith(inner, "mailto:")) return true;
    return inner.find('@') != std::string_view::npos && inner.find('.') != std::string_view::npos;
}

/// Handles "[label](target)" and "![label](target)".  `i` points at '[' or '!'.
bool parseLinkOrImage(std::string_view text, std::size_t& i, std::vector<Inline>& out,
                      std::string& pending, bool isImage) {
    const std::size_t labelStart = i + (isImage ? 2 : 1);
    if (labelStart > text.size()) return false;

    // Find the matching ']' while respecting nesting and escapes.
    std::size_t depth = 0;
    std::size_t cursor = labelStart;
    std::size_t labelEnd = kNoPosition;
    while (cursor < text.size()) {
        const char c = text[cursor];
        if (c == '\\' && cursor + 1 < text.size()) {
            cursor += 2;
            continue;
        }
        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            if (depth == 0) {
                labelEnd = cursor;
                break;
            }
            --depth;
        } else if (c == '\n') {
            return false;  // no lazy multiline links
        }
        ++cursor;
    }
    if (labelEnd == kNoPosition) return false;

    cursor = labelEnd + 1;
    if (cursor >= text.size() || text[cursor] != '(') return false;
    ++cursor;

    while (cursor < text.size() && isSpaceByte(text[cursor])) ++cursor;

    std::string href;
    if (cursor < text.size() && text[cursor] == '<') {
        const std::size_t close = text.find('>', cursor + 1);
        if (close == kNoPosition) return false;
        href = std::string(text.substr(cursor + 1, close - cursor - 1));
        cursor = close + 1;
    } else {
        const std::size_t start = cursor;
        int parentheses = 0;
        while (cursor < text.size()) {
            const char c = text[cursor];
            if (c == '\\' && cursor + 1 < text.size()) {
                cursor += 2;
                continue;
            }
            if (c == '(') {
                ++parentheses;
            } else if (c == ')') {
                if (parentheses == 0) break;
                --parentheses;
            } else if (isSpaceByte(c) && parentheses == 0) {
                break;
            }
            ++cursor;
        }
        href = std::string(text.substr(start, cursor - start));
    }

    while (cursor < text.size() && isSpaceByte(text[cursor])) ++cursor;

    std::string title;
    if (cursor < text.size() &&
        (text[cursor] == '"' || text[cursor] == '\'' || text[cursor] == '(')) {
        const char open = text[cursor];
        const char close = open == '(' ? ')' : open;
        ++cursor;
        const std::size_t start = cursor;
        while (cursor < text.size() && text[cursor] != close) ++cursor;
        if (cursor >= text.size()) return false;
        title = std::string(text.substr(start, cursor - start));
        ++cursor;
    }

    if (cursor >= text.size() || text[cursor] != ')') return false;
    ++cursor;

    flushText(pending, out);
    Inline in;
    in.type = isImage ? InlineType::Image : InlineType::Link;
    in.href = std::move(href);
    in.title = std::move(title);
    in.children = parseInlines(text.substr(labelStart, labelEnd - labelStart));
    out.push_back(std::move(in));
    i = cursor;
    return true;
}

// ---------------------------------------------------------------------------
// Block parsing
// ---------------------------------------------------------------------------

std::vector<Block> parseBlocks(const std::vector<std::string>& lines, std::size_t begin, std::size_t end);

/// Parses a run of list items once the first marker has been recognised.
void parseList(const std::vector<std::string>& lines, std::size_t& index, std::size_t end,
               const ListMarker& first, std::vector<Block>& out) {
    struct Item {
        std::vector<std::string> lines;
        bool task = false;
        bool checked = false;
    };

    const bool ordered = first.ordered;
    const char bullet = first.bullet;
    const int listIndent = first.indent;

    std::vector<Item> items;
    Item current;
    current.task = first.task;
    current.checked = first.checked;
    current.lines.push_back(std::string(lines[index].substr(first.contentOffset)));
    int contentIndent = first.contentIndent;
    std::size_t i = index + 1;

    while (i < end) {
        const std::string& line = lines[i];

        ListMarker marker = scanListMarker(line);
        if (marker.valid && marker.indent == listIndent && marker.ordered == ordered &&
            (ordered || marker.bullet == bullet)) {
            items.push_back(std::move(current));
            current = Item{};
            current.task = marker.task;
            current.checked = marker.checked;
            current.lines.push_back(std::string(line.substr(marker.contentOffset)));
            contentIndent = marker.contentIndent;
            ++i;
            continue;
        }

        if (isBlank(line)) {
            // A blank line may end the list or just separate the item's own
            // paragraphs; look ahead to decide.
            std::size_t lookahead = i;
            while (lookahead < end && isBlank(lines[lookahead])) ++lookahead;
            if (lookahead >= end) {
                i = lookahead;
                break;
            }
            ListMarker next = scanListMarker(lines[lookahead]);
            const int nextIndent = indentColumns(lines[lookahead]);
            const bool stillInList =
                (next.valid && next.indent == listIndent && next.ordered == ordered) ||
                nextIndent >= contentIndent;
            if (!stillInList) break;

            for (std::size_t k = i; k < lookahead; ++k) current.lines.emplace_back();
            i = lookahead;
            continue;
        }

        const int indent = indentColumns(line);
        if (indent >= contentIndent) {
            const std::size_t offset = byteOffsetForIndent(line, contentIndent);
            current.lines.push_back(std::string(line.substr(std::min(offset, line.size()))));
            ++i;
            continue;
        }

        // Not indented far enough and not a new marker: the list ends here.
        break;
    }

    if (!current.lines.empty()) items.push_back(std::move(current));

    Block list;
    list.type = BlockType::List;
    list.ordered = ordered;
    list.start = first.start;
    list.marker = bullet;

    for (Item& item : items) {
        while (!item.lines.empty() && isBlank(item.lines.back())) item.lines.pop_back();
        Block node;
        node.type = BlockType::ListItem;
        node.task = item.task;
        node.checked = item.checked;
        node.children = parseBlocks(item.lines, 0, item.lines.size());
        list.children.push_back(std::move(node));
    }

    out.push_back(std::move(list));
    index = i;
}

std::string joinLines(const std::vector<std::string>& lines, std::string_view separator, std::size_t begin,
                      std::size_t end) {
    std::string joined;
    for (std::size_t i = begin; i < end; ++i) {
        if (i > begin) joined.append(separator);
        joined.append(lines[i]);
    }
    return joined;
}

std::vector<Block> parseBlocks(const std::vector<std::string>& lines, std::size_t begin, std::size_t end) {
    std::vector<Block> blocks;
    std::vector<std::string> paragraph;

    const auto flushParagraph = [&]() {
        if (paragraph.empty()) return;
        Block block;
        block.type = BlockType::Paragraph;
        block.inlines = parseInlines(joinLines(paragraph, "\n", 0, paragraph.size()));
        blocks.push_back(std::move(block));
        paragraph.clear();
    };

    std::size_t i = begin;
    while (i < end) {
        const std::string& line = lines[i];

        if (isBlank(line)) {
            flushParagraph();
            ++i;
            continue;
        }

        // --- fenced code --------------------------------------------------
        if (FenceInfo fence = scanFence(line); fence.isFence) {
            flushParagraph();
            std::vector<std::string> code;
            ++i;
            while (i < end) {
                if (isFenceEnd(lines[i], fence.marker, fence.length)) {
                    ++i;
                    break;
                }
                code.push_back(lines[i]);
                ++i;
            }
            Block block;
            block.type = BlockType::CodeBlock;
            block.info = fence.info;
            block.literal = joinLines(code, "\n", 0, code.size());
            blocks.push_back(std::move(block));
            continue;
        }

        // --- setext heading (needs a pending paragraph) -------------------
        if (!paragraph.empty()) {
            if (const int level = setextLevel(line); level > 0) {
                Block block;
                block.type = BlockType::Heading;
                block.level = level;
                block.inlines = parseInlines(joinLines(paragraph, " ", 0, paragraph.size()));
                blocks.push_back(std::move(block));
                paragraph.clear();
                ++i;
                continue;
            }
        }

        // --- ATX heading --------------------------------------------------
        {
            std::string_view content;
            if (const int level = atxHeadingLevel(line, &content); level > 0) {
                flushParagraph();
                Block block;
                block.type = BlockType::Heading;
                block.level = level;
                block.inlines = parseInlines(content);
                blocks.push_back(std::move(block));
                ++i;
                continue;
            }
        }

        // --- thematic break ----------------------------------------------
        if (paragraph.empty() && isThematicBreak(line)) {
            Block block;
            block.type = BlockType::ThematicBreak;
            blocks.push_back(std::move(block));
            ++i;
            continue;
        }

        // --- block quote --------------------------------------------------
        {
            std::string_view quoted;
            if (stripBlockQuoteMarker(line, quoted)) {
                flushParagraph();
                std::vector<std::string> inner;
                inner.emplace_back(quoted);
                ++i;
                while (i < end) {
                    std::string_view nested;
                    if (stripBlockQuoteMarker(lines[i], nested)) {
                        inner.emplace_back(nested);
                        ++i;
                        continue;
                    }
                    // Lazy continuation of a paragraph inside the quote.
                    if (!isBlank(lines[i]) && !inner.empty() && !isBlank(inner.back())) {
                        inner.push_back(lines[i]);
                        ++i;
                        continue;
                    }
                    break;
                }
                Block block;
                block.type = BlockType::BlockQuote;
                block.children = parseBlocks(inner, 0, inner.size());
                blocks.push_back(std::move(block));
                continue;
            }
        }

        // --- GFM table (header row already collected in `paragraph`) ------
        if (paragraph.size() == 1 && line.find('|') != std::string::npos) {
            std::vector<Alignment> alignments;
            if (parseTableDelimiter(line, alignments)) {
                std::vector<std::string> header;
                splitTableRow(paragraph.front(), header);
                if (header.size() == alignments.size()) {
                    paragraph.clear();
                    Block block;
                    block.type = BlockType::Table;
                    block.alignments = alignments;

                    const auto makeRow = [&](const std::vector<std::string>& cells) {
                        std::vector<TableCell> row;
                        row.reserve(alignments.size());
                        for (std::size_t c = 0; c < alignments.size(); ++c) {
                            TableCell cell;
                            cell.align = alignments[c];
                            cell.inlines =
                                parseInlines(c < cells.size() ? cells[c] : std::string());
                            row.push_back(std::move(cell));
                        }
                        return row;
                    };

                    block.rows.push_back(makeRow(header));
                    ++i;
                    while (i < end && !isBlank(lines[i]) && lines[i].find('|') != std::string::npos) {
                        std::vector<std::string> cells;
                        splitTableRow(lines[i], cells);
                        block.rows.push_back(makeRow(cells));
                        ++i;
                    }
                    blocks.push_back(std::move(block));
                    continue;
                }
            }
        }

        // --- list ---------------------------------------------------------
        if (ListMarker marker = scanListMarker(line); marker.valid) {
            flushParagraph();
            parseList(lines, i, end, marker, blocks);
            continue;
        }

        // --- indented code block ------------------------------------------
        // An indented block can never interrupt a paragraph, hence the guard.
        if (paragraph.empty() && indentColumns(line) >= 4) {
            std::vector<std::string> code;
            while (i < end) {
                if (isBlank(lines[i])) {
                    std::size_t lookahead = i;
                    while (lookahead < end && isBlank(lines[lookahead])) ++lookahead;
                    if (lookahead >= end || indentColumns(lines[lookahead]) < 4) break;
                    for (std::size_t k = i; k < lookahead; ++k) code.emplace_back();
                    i = lookahead;
                    continue;
                }
                if (indentColumns(lines[i]) < 4) break;
                code.push_back(std::string(lines[i].substr(byteOffsetForIndent(lines[i], 4))));
                ++i;
            }
            Block block;
            block.type = BlockType::CodeBlock;
            block.literal = joinLines(code, "\n", 0, code.size());
            blocks.push_back(std::move(block));
            continue;
        }

        // --- paragraph continuation ---------------------------------------
        paragraph.push_back(std::string(str::trim(line)));
        ++i;
    }

    flushParagraph();
    return blocks;
}

}  // namespace

std::vector<Inline> parseInlines(std::string_view text) {
    std::vector<Inline> out;
    std::string pending;

    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];

        // --- backslash escapes and hard breaks ---------------------------
        if (c == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == '\n') {
                flushText(pending, out);
                Inline br;
                br.type = InlineType::LineBreak;
                out.push_back(std::move(br));
                i += 2;
                continue;
            }
            if (isAsciiPunctuation(next)) {
                pending.push_back(next);
                i += 2;
                continue;
            }
        }

        // --- line breaks --------------------------------------------------
        if (c == '\n') {
            std::size_t trailingSpaces = 0;
            while (trailingSpaces < pending.size() &&
                   pending[pending.size() - 1 - trailingSpaces] == ' ') {
                ++trailingSpaces;
            }
            Inline br;
            if (trailingSpaces >= 2) {
                pending.resize(pending.size() - trailingSpaces);
                br.type = InlineType::LineBreak;
            } else {
                br.type = InlineType::SoftBreak;
            }
            flushText(pending, out);
            out.push_back(std::move(br));
            ++i;
            continue;
        }

        // --- code span ----------------------------------------------------
        if (c == '`') {
            std::size_t run = 0;
            while (i + run < text.size() && text[i + run] == '`') ++run;
            const std::string_view opener = text.substr(i, run);
            const std::size_t closer = text.find(opener, i + run);
            if (closer != kNoPosition) {
                std::string_view content = text.substr(i + run, closer - (i + run));
                if (content.size() >= 2 && content.front() == ' ' && content.back() == ' ' &&
                    str::trim(content).size() > 1) {
                    content = content.substr(1, content.size() - 2);
                }
                flushText(pending, out);
                Inline code;
                code.type = InlineType::Code;
                code.text = str::replaceAll(content, "\n", " ");
                out.push_back(std::move(code));
                i = closer + run;
                continue;
            }
        }

        // --- images and links ---------------------------------------------
        if (c == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            std::size_t cursor = i;
            if (parseLinkOrImage(text, cursor, out, pending, true)) {
                i = cursor;
                continue;
            }
        }
        if (c == '[') {
            std::size_t cursor = i;
            if (parseLinkOrImage(text, cursor, out, pending, false)) {
                i = cursor;
                continue;
            }
        }

        // --- autolinks -----------------------------------------------------
        if (c == '<') {
            const std::size_t close = text.find('>', i + 1);
            if (close != kNoPosition) {
                const std::string_view inner = text.substr(i + 1, close - i - 1);
                if (looksLikeAutolink(inner)) {
                    flushText(pending, out);
                    Inline link;
                    link.type = InlineType::Link;
                    link.href = std::string(inner);
                    Inline label;
                    label.type = InlineType::Text;
                    label.text = std::string(inner);
                    link.children.push_back(std::move(label));
                    out.push_back(std::move(link));
                    i = close + 1;
                    continue;
                }
            }
        }

        // --- emphasis ------------------------------------------------------
        {
            const auto tryEmphasis = [&](std::string_view delimiter, InlineType type,
                                         bool underscoreRule) -> bool {
                if (text.compare(i, delimiter.size(), delimiter) != 0) return false;
                const std::size_t inner = i + delimiter.size();
                if (inner >= text.size() || isSpaceByte(text[inner])) return false;
                if (underscoreRule && i > 0 && isWordByte(text[i - 1])) return false;

                const std::size_t closer =
                    findClosingDelimiter(text, inner, delimiter, underscoreRule);
                if (closer == kNoPosition) return false;

                flushText(pending, out);
                Inline node;
                node.type = type;
                node.children = parseInlines(text.substr(inner, closer - inner));
                out.push_back(std::move(node));
                i = closer + delimiter.size();
                return true;
            };

            if (tryEmphasis("**", InlineType::Strong, false)) continue;
            if (tryEmphasis("__", InlineType::Strong, true)) continue;
            if (tryEmphasis("~~", InlineType::Strikethrough, false)) continue;
            if (tryEmphasis("*", InlineType::Emphasis, false)) continue;
            if (tryEmphasis("_", InlineType::Emphasis, true)) continue;
        }

        pending.push_back(c);
        ++i;
    }

    flushText(pending, out);
    return out;
}

Document parseMarkdown(std::string_view text) {
    Document document;
    const std::vector<std::string> lines = str::splitLines(text);
    document.blocks = parseBlocks(lines, 0, lines.size());
    return document;
}

}  // namespace mdview::markdown
