#include "markdown/highlight.hpp"

#include "utils/string_utils.hpp"

#include <cctype>
#include <string>
#include <unordered_set>

namespace mdview::markdown {

namespace {

struct WordSet {
    std::string_view storage;
    std::unordered_set<std::string_view> words;

    explicit WordSet(std::string_view text) : storage(text) {
        std::size_t position = 0;
        while (position < storage.size()) {
            const std::size_t separator = storage.find(' ', position);
            const std::size_t stop =
                separator == std::string_view::npos ? storage.size() : separator;
            if (stop > position) words.insert(storage.substr(position, stop - position));
            position = stop + 1;
        }
    }

    bool contains(std::string_view word) const { return words.find(word) != words.end(); }
};

struct Language {
    WordSet keywords;
    WordSet types;
    WordSet constants;
    bool hashComment = false;
    bool slashComment = true;
    bool blockComment = true;
    bool preprocessor = false;
    bool caseInsensitive = false;
    bool tripleQuotes = false;
    bool backtickStrings = false;
};

const Language& cLike() {
    static const Language language{
        WordSet("alignas alignof and and_eq asm auto break case catch class compl concept const "
                "consteval constexpr constinit const_cast continue co_await co_return co_yield "
                "decltype default delete do dynamic_cast else enum explicit export extern for "
                "friend goto if inline mutable namespace new noexcept not not_eq operator or "
                "or_eq private protected public register reinterpret_cast requires return static "
                "static_assert static_cast struct switch template this thread_local throw try "
                "typedef typeid typename union using virtual volatile while xor xor_eq"),
        WordSet("bool char char8_t char16_t char32_t double float int long short signed unsigned "
                "void wchar_t size_t ssize_t int8_t int16_t int32_t int64_t uint8_t uint16_t "
                "uint32_t uint64_t string vector map unordered_map set unordered_set array "
                "optional variant tuple pair unique_ptr shared_ptr weak_ptr ostream istream "
                "fstream string_view span function thread mutex"),
        WordSet("true false nullptr NULL"),
        /*hashComment=*/false,
        /*slashComment=*/true,
        /*blockComment=*/true,
        /*preprocessor=*/true,  // '#include', '#define', ...
        /*caseInsensitive=*/false,
        /*tripleQuotes=*/false,
        /*backtickStrings=*/false,
    };
    return language;
}

const Language& python() {
    static const Language language{
        WordSet("and as assert async await break class continue def del elif else except finally "
                "for from global if import in is lambda nonlocal not or pass raise return try "
                "while with yield match case"),
        WordSet("int float complex str bool list dict tuple set frozenset bytes bytearray object "
                "type Exception ValueError TypeError self cls"),
        WordSet("True False None Ellipsis NotImplemented"),
        /*hashComment=*/true,
        /*slashComment=*/false,
        /*blockComment=*/false,
        /*preprocessor=*/false,
        /*caseInsensitive=*/false,
        /*tripleQuotes=*/true,
    };
    return language;
}

const Language& javascript() {
    static const Language language{
        WordSet("var let const function return if else for while do switch case break continue new "
                "delete typeof instanceof in of class extends super this try catch finally throw "
                "async await yield import export default from as static get set void"),
        WordSet("string number boolean any unknown never object symbol bigint Array Object Promise "
                "Map Set Date RegExp Error console JSON Math document window"),
        WordSet("true false null undefined NaN Infinity"),
        /*hashComment=*/false,
        /*slashComment=*/true,
        /*blockComment=*/true,
        /*preprocessor=*/false,
        /*caseInsensitive=*/false,
        /*tripleQuotes=*/false,
        /*backtickStrings=*/true,
    };
    return language;
}

const Language& shell() {
    static const Language language{
        WordSet("if then else elif fi for while until do done case esac function in return local "
                "export readonly declare unset shift echo printf read cd exit set trap source "
                "alias eval exec test"),
        WordSet(""),
        WordSet("true false"),
        /*hashComment=*/true,
        /*slashComment=*/false,
        /*blockComment=*/false,
    };
    return language;
}

const Language& jsonLike() {
    static const Language language{
        WordSet(""),
        WordSet(""),
        WordSet("true false null"),
        /*hashComment=*/false,
        /*slashComment=*/false,
        /*blockComment=*/false,
    };
    return language;
}

const Language& yamlLike() {
    static const Language language{
        WordSet(""),
        WordSet(""),
        WordSet("true false null yes no on off"),
        /*hashComment=*/true,
        /*slashComment=*/false,
        /*blockComment=*/false,
    };
    return language;
}

const Language& cssLike() {
    static const Language language{
        WordSet("important media supports keyframes font-face import charset namespace page"),
        WordSet(""),
        WordSet(""),
        /*hashComment=*/false,
        /*slashComment=*/true,
        /*blockComment=*/true,
    };
    return language;
}

const Language& sql() {
    static const Language language{
        WordSet("select from where group by having order limit offset insert into values update "
                "set delete create table alter drop index view join left right inner outer on as "
                "and or not null is in between like distinct union all case when then else end "
                "primary key foreign references default unique cascade"),
        WordSet("int integer bigint smallint varchar char text date timestamp boolean numeric "
                "decimal serial uuid json jsonb"),
        WordSet("null true false"),
        /*hashComment=*/false,
        /*slashComment=*/true,
        /*blockComment=*/true,
        /*preprocessor=*/false,
        /*caseInsensitive=*/true,
    };
    return language;
}

const Language& markup() {
    static const Language language{
        WordSet(""),
        WordSet(""),
        WordSet(""),
        /*hashComment=*/false,
        /*slashComment=*/true,
        /*blockComment=*/true,
    };
    return language;
}

std::string normaliseLanguage(std::string_view language) {
    std::string name = str::toLowerAscii(str::trim(language));
    // The info string may carry extra words, e.g. "cpp title=example".
    const std::size_t stop = name.find_first_of(" \t,;:");
    if (stop != std::string::npos) name.resize(stop);
    return name;
}

const Language* findLanguage(std::string_view language) {
    const std::string name = normaliseLanguage(language);
    if (name.empty()) return nullptr;

    if (name == "c" || name == "h" || name == "cpp" || name == "c++" || name == "cc" ||
        name == "cxx" || name == "hpp" || name == "hxx" || name == "hh" || name == "cs" ||
        name == "csharp" || name == "java" || name == "kt" || name == "kotlin" ||
        name == "swift" || name == "go" || name == "golang" || name == "rs" || name == "rust" ||
        name == "php" || name == "dart" || name == "scala" || name == "m" || name == "mm") {
        return &cLike();
    }
    if (name == "py" || name == "python" || name == "python3" || name == "rb" || name == "ruby") {
        return &python();
    }
    if (name == "js" || name == "javascript" || name == "jsx" || name == "ts" || name == "tsx" ||
        name == "typescript" || name == "mjs" || name == "cjs") {
        return &javascript();
    }
    if (name == "sh" || name == "bash" || name == "zsh" || name == "shell" || name == "console" ||
        name == "powershell" || name == "ps1" || name == "bat" || name == "cmd") {
        return &shell();
    }
    if (name == "json" || name == "json5" || name == "jsonc") return &jsonLike();
    if (name == "yaml" || name == "yml" || name == "toml" || name == "ini" || name == "conf") {
        return &yamlLike();
    }
    if (name == "css" || name == "scss" || name == "sass" || name == "less") return &cssLike();
    if (name == "sql" || name == "postgres" || name == "psql" || name == "mysql") return &sql();
    if (name == "html" || name == "htm" || name == "xml" || name == "svg" || name == "vue" ||
        name == "md" || name == "markdown") {
        return &markup();
    }
    return &cLike();  // generic fallback still finds comments, strings and numbers
}

bool isIdentifierStart(unsigned char c) {
    return std::isalpha(c) != 0 || c == '_' || c == '$';
}

bool isIdentifierBody(unsigned char c) { return std::isalnum(c) != 0 || c == '_' || c == '$'; }

/// Finds the end of a quoted string, honouring backslash escapes.
std::size_t scanQuoted(std::string_view code, std::size_t start, char quote, bool allowNewline) {
    std::size_t i = start + 1;
    while (i < code.size()) {
        const char c = code[i];
        if (c == '\\' && i + 1 < code.size()) {
            i += 2;
            continue;
        }
        if (c == quote) return i + 1;
        if (c == '\n' && !allowNewline) return i;
        ++i;
    }
    return code.size();
}

bool isOnlyWhitespace(std::string_view text) {
    for (const char c : text) {
        if (c != ' ' && c != '\t') return false;
    }
    return true;
}

}  // namespace

bool isKnownLanguage(std::string_view language) {
    const std::string name = normaliseLanguage(language);
    if (name.empty()) return false;
    static const char* const kKnown[] = {
        "c",    "h",     "cpp",  "c++",  "cc",   "cxx",    "hpp",   "hxx",  "hh",
        "cs",   "csharp", "java", "kt",  "kotlin", "swift", "go",   "golang", "rs",
        "rust", "php",   "dart", "scala", "py",  "python", "rb",    "ruby", "js",
        "javascript", "jsx", "ts", "tsx", "typescript", "sh", "bash", "zsh", "shell",
        "console", "powershell", "ps1", "bat", "cmd", "json", "yaml", "yml", "toml",
        "ini",  "css",   "scss", "sql",  "html", "xml",    "svg",   "md",   "markdown",
    };
    for (const char* candidate : kKnown) {
        if (name == candidate) return true;
    }
    return false;
}

std::vector<CodeToken> highlightCode(std::string_view code, std::string_view language) {
    static const Language kFallback = cLike();
    const Language* found = findLanguage(language);
    const Language& active = found ? *found : kFallback;

    std::vector<CodeToken> tokens;
    std::size_t plainStart = 0;

    const auto pushToken = [&](CodeTokenKind kind, std::size_t begin, std::size_t end) {
        if (end <= begin) return;
        if (begin > plainStart) {
            tokens.push_back(CodeToken{CodeTokenKind::Plain, plainStart, begin});
        }
        tokens.push_back(CodeToken{kind, begin, end});
        plainStart = end;
    };

    std::size_t lineStart = 0;
    std::size_t i = 0;
    while (i < code.size()) {
        const char c = code[i];

        if (c == '\n') {
            ++i;
            lineStart = i;
            continue;
        }

        // Preprocessor directives only count at the start of a line.
        if (active.preprocessor && c == '#' && isOnlyWhitespace(code.substr(lineStart, i - lineStart))) {
            const std::size_t end = code.find('\n', i);
            pushToken(CodeTokenKind::Preprocessor, i, end == std::string_view::npos ? code.size() : end);
            i = end == std::string_view::npos ? code.size() : end;
            continue;
        }

        if (active.blockComment && c == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            std::size_t end = code.find("*/", i + 2);
            end = end == std::string_view::npos ? code.size() : end + 2;
            pushToken(CodeTokenKind::Comment, i, end);
            i = end;
            continue;
        }

        if (active.slashComment && c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            const std::size_t end = code.find('\n', i);
            pushToken(CodeTokenKind::Comment, i, end == std::string_view::npos ? code.size() : end);
            i = end == std::string_view::npos ? code.size() : end;
            continue;
        }

        if (active.hashComment && c == '#') {
            const std::size_t end = code.find('\n', i);
            pushToken(CodeTokenKind::Comment, i, end == std::string_view::npos ? code.size() : end);
            i = end == std::string_view::npos ? code.size() : end;
            continue;
        }

        if (active.tripleQuotes && (c == '"' || c == '\'') && i + 2 < code.size() &&
            code[i + 1] == c && code[i + 2] == c) {
            const std::string_view triple = code.substr(i, 3);
            const std::size_t end = code.find(triple, i + 3);
            const std::size_t stop = end == std::string_view::npos ? code.size() : end + 3;
            pushToken(CodeTokenKind::String, i, stop);
            i = stop;
            continue;
        }

        if (c == '"' || c == '\'' || (active.backtickStrings && c == '`')) {
            const bool allowNewline = active.backtickStrings && c == '`';
            const std::size_t end = scanQuoted(code, i, c, allowNewline);
            pushToken(CodeTokenKind::String, i, end);
            i = end;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) != 0 ||
            (c == '.' && i + 1 < code.size() &&
             std::isdigit(static_cast<unsigned char>(code[i + 1])) != 0)) {
            std::size_t end = i;
            bool seenExponent = false;
            while (end < code.size()) {
                const char d = code[end];
                if (std::isalnum(static_cast<unsigned char>(d)) != 0 || d == '.' || d == '_' ||
                    ((d == '+' || d == '-') && seenExponent)) {
                    seenExponent = (d == 'e' || d == 'E') && !seenExponent;
                    ++end;
                    continue;
                }
                break;
            }
            pushToken(CodeTokenKind::Number, i, end);
            i = end;
            continue;
        }

        if (isIdentifierStart(static_cast<unsigned char>(c))) {
            std::size_t end = i;
            while (end < code.size() && isIdentifierBody(static_cast<unsigned char>(code[end]))) ++end;

            const std::string_view word = code.substr(i, end - i);
            std::string lowered;
            std::string_view lookup = word;
            if (active.caseInsensitive) {
                lowered = str::toLowerAscii(word);
                lookup = lowered;
            }

            CodeTokenKind kind = CodeTokenKind::Plain;
            if (active.constants.contains(lookup)) {
                kind = CodeTokenKind::Constant;
            } else if (active.keywords.contains(lookup)) {
                kind = CodeTokenKind::Keyword;
            } else if (active.types.contains(lookup)) {
                kind = CodeTokenKind::Type;
            } else {
                // A call-like identifier, i.e. followed by '(' after spaces.
                std::size_t lookahead = end;
                while (lookahead < code.size() && (code[lookahead] == ' ' || code[lookahead] == '\t')) {
                    ++lookahead;
                }
                if (lookahead < code.size() && code[lookahead] == '(') kind = CodeTokenKind::Function;
                // Conventional constant naming (ALL_CAPS) reads better as a constant.
                else if (word.size() > 1 && std::isupper(static_cast<unsigned char>(word.front())) != 0 &&
                         word.find('_') != std::string_view::npos) {
                    kind = CodeTokenKind::Constant;
                }
            }

            pushToken(kind, i, end);
            i = end;
            continue;
        }

        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' || c == '!' ||
            c == '<' || c == '>' || c == '&' || c == '|' || c == '^' || c == '~' || c == '?') {
            std::size_t end = i;
            while (end < code.size()) {
                const char d = code[end];
                if (d == '+' || d == '-' || d == '*' || d == '/' || d == '%' || d == '=' || d == '!' ||
                    d == '<' || d == '>' || d == '&' || d == '|' || d == '^' || d == '~' || d == '?') {
                    ++end;
                    continue;
                }
                break;
            }
            // "//" and "/*" were handled above, so a run here is a real operator.
            pushToken(CodeTokenKind::Operator, i, end);
            i = end;
            continue;
        }

        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == ',' ||
            c == ';' || c == ':') {
            pushToken(CodeTokenKind::Punctuation, i, i + 1);
            ++i;
            continue;
        }

        ++i;  // whitespace and anything else stays Plain
    }

    if (plainStart < code.size()) {
        tokens.push_back(CodeToken{CodeTokenKind::Plain, plainStart, code.size()});
    }
    return tokens;
}

}  // namespace mdview::markdown
