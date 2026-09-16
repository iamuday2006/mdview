#include "test_framework.hpp"

#include "utils/geometry.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include "utils/unicode.hpp"

#include <string>
#include <vector>

using namespace mdview;

namespace {

std::vector<std::string> toVector(const std::vector<std::string_view>& views) {
    std::vector<std::string> out;
    out.reserve(views.size());
    for (const std::string_view view : views) out.emplace_back(view);
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// unicode
// ---------------------------------------------------------------------------

MDVIEW_TEST(unicode, decode_lengths) {
    char32_t codePoint = 0;
    CHECK_EQ(uni::decode("A", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0x41u);

    CHECK_EQ(uni::decode("\xC3\xA9", 0, codePoint), std::size_t{2});  // e-acute
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xE9u);

    CHECK_EQ(uni::decode("\xE6\xBC\xA2", 0, codePoint), std::size_t{3});  // CJK
    CHECK_EQ(static_cast<unsigned>(codePoint), 0x6F22u);

    CHECK_EQ(uni::decode("\xF0\x9F\x98\x80", 0, codePoint), std::size_t{4});  // emoji
    CHECK_EQ(static_cast<unsigned>(codePoint), 0x1F600u);
}

MDVIEW_TEST(unicode, decode_rejects_malformed_input) {
    char32_t codePoint = 0;

    // A bare continuation byte and an invalid lead byte both yield U+FFFD and
    // consume exactly one byte, so iteration always advances.
    CHECK_EQ(uni::decode("\x80", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xFFFDu);
    CHECK_EQ(uni::decode("\xFF", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xFFFDu);

    // Overlong encoding of '/' (0xC0 0xAF) must be rejected.
    CHECK_EQ(uni::decode("\xC0\xAF", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xFFFDu);

    // Surrogate half (0xED 0xA0 0x80 == U+D800) must be rejected.
    CHECK_EQ(uni::decode("\xED\xA0\x80", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xFFFDu);

    // Truncated sequence at the end of the buffer.
    CHECK_EQ(uni::decode("\xE6\xBC", 0, codePoint), std::size_t{1});
    CHECK_EQ(static_cast<unsigned>(codePoint), 0xFFFDu);
}

MDVIEW_TEST(unicode, encode_round_trip) {
    const char32_t samples[] = {U'A', 0x00E9, 0x6F22, 0x1F600, 0x7F};
    for (const char32_t sample : samples) {
        const std::string encoded = uni::encodeUtf8(sample);
        char32_t decoded = 0;
        const std::size_t consumed = uni::decode(encoded, 0, decoded);
        CHECK_EQ(consumed, encoded.size());
        CHECK_EQ(static_cast<unsigned>(decoded), static_cast<unsigned>(sample));
    }
}

MDVIEW_TEST(unicode, display_width) {
    CHECK_EQ(uni::codepointWidth(U'a'), 1);
    CHECK_EQ(uni::codepointWidth(0x6F22), 2);        // 漢
    CHECK_EQ(uni::codepointWidth(0x0301), 0);        // combining acute
    CHECK_EQ(uni::codepointWidth(0x200B), 0);        // zero width space
    CHECK_EQ(uni::codepointWidth(0x1F600), 2);       // emoji
    CHECK_EQ(uni::displayWidth("abc"), 3);
    CHECK_EQ(uni::displayWidth("\xE6\xBC\xA2\xE5\xAD\x97"), 4);  // 漢字
    CHECK_EQ(uni::countCodepoints("\xE6\xBC\xA2\xE5\xAD\x97"), std::size_t{2});
}

MDVIEW_TEST(unicode, truncate_and_pad) {
    CHECK_EQ(uni::truncate("hello world", 5, ""), std::string("hello"));
    CHECK_EQ(uni::truncate("hello", 10, "..."), std::string("hello"));
    // The ellipsis counts towards the budget.
    CHECK_EQ(uni::displayWidth(uni::truncate("hello world", 6, "...")), 6);
    CHECK_EQ(uni::truncate("", 4, "..."), std::string(""));
    CHECK_EQ(uni::truncate("abc", 0, "..."), std::string(""));
    // Wide characters are not split in half.
    CHECK_EQ(uni::displayWidth(uni::truncate("\xE6\xBC\xA2\xE6\xBC\xA2\xE6\xBC\xA2", 3, "")), 2);
    CHECK_EQ(uni::padRight("ab", 5), std::string("ab   "));
    CHECK_EQ(uni::padRight("abcdef", 3), std::string("abcdef"));
    CHECK_EQ(uni::byteOffsetForColumn("\xE6\xBC\xA2\xE6\xBC\xA2", 2), std::size_t{3});
}

// ---------------------------------------------------------------------------
// string_utils
// ---------------------------------------------------------------------------

MDVIEW_TEST(str, trimming_and_predicates) {
    CHECK_EQ(str::trim("  hi  "), std::string_view("hi"));
    CHECK_EQ(str::trimLeft("\t x"), std::string_view("x"));
    CHECK_EQ(str::trimRight("x \t"), std::string_view("x"));
    CHECK(str::isBlank("   \t "));
    CHECK(!str::isBlank(" a "));
    CHECK(str::startsWith("hello", "he"));
    CHECK(!str::startsWith("he", "hello"));
    CHECK(str::endsWith("hello", "lo"));
    CHECK(!str::endsWith("hello", "ol"));
}

MDVIEW_TEST(str, case_helpers) {
    CHECK_EQ(str::toLowerAscii("AbC"), std::string("abc"));
    CHECK_EQ(str::toUpperAscii("AbC"), std::string("ABC"));
    CHECK(str::equalsIgnoreCaseAscii("ReadMe.MD", "readme.md"));
    CHECK(str::compareIgnoreCaseAscii("a", "B") < 0);
    CHECK_EQ(str::compareIgnoreCaseAscii("abc", "ABC"), 0);
}

MDVIEW_TEST(str, splitting) {
    CHECK_EQ(toVector(str::split("a,b,c", ',')), (std::vector<std::string>{"a", "b", "c"}));
    CHECK_EQ(toVector(str::split("a,,b", ',')), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(toVector(str::split("a,,b", ',', true)), (std::vector<std::string>{"a", "", "b"}));

    CHECK_EQ(str::splitLines("a\nb"), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(str::splitLines("a\r\nb"), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(str::splitLines("a\rb"), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(str::splitLines("a\n"), (std::vector<std::string>{"a"}));
    CHECK_EQ(str::splitLines("a\n\nb"), (std::vector<std::string>{"a", "", "b"}));
    CHECK(str::splitLines("").empty());
}

MDVIEW_TEST(str, replace_repeat_and_padding) {
    CHECK_EQ(str::replaceAll("a-b-c", "-", "+"), std::string("a+b+c"));
    CHECK_EQ(str::replaceAll("aaa", "aa", "b"), std::string("ba"));
    CHECK_EQ(str::repeat("ab", 2), std::string("abab"));
    CHECK_EQ(str::padLeftAscii("7", 3), std::string("  7"));
    CHECK_EQ(str::padRightAscii("7", 3), std::string("7  "));
}

MDVIEW_TEST(str, strips_indent) {
    int columns = 0;
    CHECK_EQ(str::stripIndent("    code", columns), std::string_view("code"));
    CHECK_EQ(columns, 4);
    CHECK_EQ(str::stripIndent("no indent", columns), std::string_view("no indent"));
    CHECK_EQ(columns, 0);
}

// ---------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------

MDVIEW_TEST(geometry, rect_predicates) {
    const Rect rect{2, 3, 4, 5};
    CHECK_EQ(rect.right(), 6);
    CHECK_EQ(rect.bottom(), 8);
    CHECK(rect.contains(2, 3));
    CHECK(rect.contains(5, 7));
    CHECK(!rect.contains(6, 7));
    CHECK(!rect.contains(2, 2));
    // Extra parentheses keep the preprocessor from splitting on the commas.
    CHECK((rect.inset(1) == Rect{3, 4, 2, 3}));
}

MDVIEW_TEST(geometry, intersection) {
    const Rect a{0, 0, 10, 10};
    const Rect b{5, 5, 10, 10};
    CHECK((a.intersected(b) == Rect{5, 5, 5, 5}));
    CHECK(a.intersected(Rect{20, 20, 1, 1}).empty());
}

MDVIEW_TEST(geometry, carving) {
    Rect area{0, 0, 20, 10};
    const Rect left = carveLeft(area, 6);
    CHECK((left == Rect{0, 0, 6, 10}));
    CHECK((area == Rect{6, 0, 14, 10}));

    const Rect top = carveTop(area, 2);
    CHECK((top == Rect{6, 0, 14, 2}));
    CHECK((area == Rect{6, 2, 14, 8}));

    const Rect bottom = carveBottom(area, 1);
    CHECK((bottom == Rect{6, 9, 14, 1}));
    CHECK((area == Rect{6, 2, 14, 7}));

    const Rect right = carveRight(area, 4);
    CHECK((right == Rect{16, 2, 4, 7}));

    // Over-large carves clamp instead of producing negative sizes.
    Rect tiny{0, 0, 3, 3};
    CHECK(carveLeft(tiny, 99).width == 3);
    CHECK(tiny.width == 0);
}

// ---------------------------------------------------------------------------
// time
// ---------------------------------------------------------------------------

MDVIEW_TEST(time, monotonic_clock_advances) {
    const std::uint64_t before = monotonicMillis();
    sleepMillis(12);
    const std::uint64_t after = monotonicMillis();
    CHECK(after >= before);
    CHECK(after - before >= 5);
}
