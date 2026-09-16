#include "test_framework.hpp"

#include "terminal/input.hpp"
#include "terminal/screen.hpp"
#include "terminal/style.hpp"
#include "terminal/terminal.hpp"

#include <string>
#include <vector>

using namespace mdview;
using mdview::terminal::Key;
using mdview::terminal::KeyEvent;
using mdview::terminal::InputDecoder;

namespace {

std::vector<KeyEvent> decode(const std::string& bytes) {
    InputDecoder decoder;
    std::vector<KeyEvent> events = decoder.feed(bytes);
    for (const KeyEvent& event : decoder.flush()) events.push_back(event);
    return events;
}

}  // namespace

// ---------------------------------------------------------------------------
// InputDecoder
// ---------------------------------------------------------------------------

MDVIEW_TEST(input, plain_characters) {
    const auto events = decode("a");
    REQUIRE(events.size() == 1);
    CHECK(events[0].key == Key::Character);
    CHECK(events[0].isPlainChar('a'));
    CHECK(!events[0].isPlainChar('A'));
    CHECK_EQ(events[0].text(), std::string("a"));
}

MDVIEW_TEST(input, utf8_characters) {
    const auto events = decode("\xE6\xBC\xA2");  // CJK
    REQUIRE(events.size() == 1);
    CHECK(events[0].key == Key::Character);
    CHECK_EQ(static_cast<unsigned>(events[0].codepoint), 0x6F22u);
}

MDVIEW_TEST(input, utf8_split_across_reads) {
    InputDecoder decoder;
    CHECK(decoder.feed("\xE6").empty());
    CHECK(decoder.pending());

    const auto events = decoder.feed("\xBC\xA2");
    REQUIRE(events.size() == 1);
    CHECK_EQ(static_cast<unsigned>(events[0].codepoint), 0x6F22u);
    CHECK(!decoder.pending());
}

MDVIEW_TEST(input, arrow_and_navigation_sequences) {
    CHECK(decode("\x1b[A")[0].key == Key::Up);
    CHECK(decode("\x1b[B")[0].key == Key::Down);
    CHECK(decode("\x1b[C")[0].key == Key::Right);
    CHECK(decode("\x1b[D")[0].key == Key::Left);
    CHECK(decode("\x1bOA")[0].key == Key::Up);
    CHECK(decode("\x1b[H")[0].key == Key::Home);
    CHECK(decode("\x1b[F")[0].key == Key::End);
    CHECK(decode("\x1b[5~")[0].key == Key::PageUp);
    CHECK(decode("\x1b[6~")[0].key == Key::PageDown);
    CHECK(decode("\x1b[3~")[0].key == Key::Delete);
    CHECK(decode("\x1b[2~")[0].key == Key::Insert);
    CHECK(decode("\x1b[1~")[0].key == Key::Home);
    CHECK(decode("\x1b[15~")[0].key == Key::F5);
    CHECK(decode("\x1b[24~")[0].key == Key::F12);
    CHECK(decode("\x1b[Z")[0].key == Key::BackTab);
}

MDVIEW_TEST(input, modifiers_on_csi_sequences) {
    const auto events = decode("\x1b[1;5C");  // Ctrl+Right
    REQUIRE(events.size() == 1);
    CHECK(events[0].key == Key::Right);
    CHECK(events[0].hasCtrl());
    CHECK(!events[0].hasAlt());

    const auto alt = decode("\x1b[1;3A");
    REQUIRE(alt.size() == 1);
    CHECK(alt[0].key == Key::Up);
    CHECK(alt[0].hasAlt());
}

MDVIEW_TEST(input, control_bytes) {
    CHECK(decode("\x03")[0].key == Key::CtrlC);
    CHECK(decode("\x04")[0].key == Key::CtrlD);
    CHECK(decode("\x0C")[0].key == Key::CtrlL);
    CHECK(decode("\x12")[0].key == Key::CtrlR);
    CHECK(decode("\x09")[0].key == Key::Tab);
    CHECK(decode("\x0D")[0].key == Key::Enter);
    CHECK(decode("\x0A")[0].key == Key::Enter);
    CHECK(decode("\x7F")[0].key == Key::Backspace);
    CHECK(decode("\x08")[0].key == Key::Backspace);

    // Ctrl+letter that has no dedicated Key value.
    const auto ctrlE = decode("\x05");
    REQUIRE(ctrlE.size() == 1);
    CHECK(ctrlE[0].key == Key::Character);
    CHECK(ctrlE[0].hasCtrl());
    CHECK_EQ(static_cast<unsigned>(ctrlE[0].codepoint), static_cast<unsigned>(U'e'));
    // isPlainChar() is about *unmodified* keys, so a Ctrl chord must not match.
    CHECK(!ctrlE[0].isPlainChar('e'));
}

MDVIEW_TEST(input, escape_and_alt) {
    // A lone ESC only becomes Escape once the decoder is flushed.
    InputDecoder decoder;
    CHECK(decoder.feed("\x1b").empty());
    CHECK(decoder.pending());
    const auto flushed = decoder.flush();
    REQUIRE(flushed.size() == 1);
    CHECK(flushed[0].key == Key::Escape);

    const auto alt = decode("\x1bx");
    REQUIRE(alt.size() == 1);
    CHECK(alt[0].key == Key::Character);
    CHECK(alt[0].hasAlt());
    CHECK_EQ(static_cast<unsigned>(alt[0].codepoint), static_cast<unsigned>(U'x'));
    CHECK(!alt[0].isPlainChar('x'));
}

MDVIEW_TEST(input, incomplete_sequence_waits_for_more_bytes) {
    InputDecoder decoder;
    CHECK(decoder.feed("\x1b[").empty());
    CHECK(decoder.pending());
    const auto events = decoder.feed("A");
    REQUIRE(events.size() == 1);
    CHECK(events[0].key == Key::Up);
}

MDVIEW_TEST(input, mouse_reports_are_ignored) {
    CHECK(decode("\x1b[<0;10;5M").empty());
    CHECK(decode("\x1b[<0;10;5m").empty());
    CHECK(decode("\x1b[?1004h").empty());
}

MDVIEW_TEST(input, several_events_in_one_feed) {
    const auto events = decode("ab");
    REQUIRE(events.size() == 2);
    CHECK(events[0].isPlainChar('a'));
    CHECK(events[1].isPlainChar('b'));
}

// ---------------------------------------------------------------------------
// Style
// ---------------------------------------------------------------------------

MDVIEW_TEST(style, plain_mode_emits_nothing) {
    const terminal::Style bold = terminal::Style{}.withBold();
    CHECK_EQ(terminal::sgrFor(bold, terminal::ColorMode::None), std::string());
}

MDVIEW_TEST(style, sgr_contains_attributes_and_reset) {
    const std::string sgr = terminal::sgrFor(terminal::Style{}.withUnderline(), terminal::ColorMode::Basic);
    CHECK_CONTAINS(sgr, "\x1b[");
    CHECK_CONTAINS(sgr, "0");
    CHECK_CONTAINS(sgr, "4");
    CHECK_EQ(sgr.back(), 'm');
}

MDVIEW_TEST(style, colour_degradation) {
    CHECK_EQ(terminal::degradeIndexedToBasic(3), 3);
    CHECK_EQ(terminal::degradeIndexedToBasic(196), 9);  // pure red in the 256 cube

    const terminal::Color degraded =
        terminal::degradeColor(terminal::Color::rgb(255, 0, 0), terminal::ColorMode::Basic);
    CHECK(degraded.kind == terminal::Color::Kind::Indexed);
    CHECK_EQ(static_cast<int>(degraded.index), 9);

    const terminal::Color none =
        terminal::degradeColor(terminal::Color::rgb(1, 2, 3), terminal::ColorMode::None);
    CHECK(none.kind == terminal::Color::Kind::Default);

    // Truecolor passes through untouched.
    const terminal::Color kept =
        terminal::degradeColor(terminal::Color::rgb(1, 2, 3), terminal::ColorMode::TrueColor);
    CHECK(kept.kind == terminal::Color::Kind::Rgb);
}

MDVIEW_TEST(style, themes_are_usable) {
    const terminal::Theme plain = terminal::Theme::plain();
    CHECK(plain.text.fg.kind == terminal::Color::Kind::Default);

    const terminal::Theme colored = terminal::Theme::colored(terminal::ColorMode::Ansi256);
    CHECK(colored.heading[0].bold);
    CHECK(colored.heading[0].fg.kind != terminal::Color::Kind::Default);
    CHECK(colored.code.keyword.bold);
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------

namespace {

terminal::Screen makeScreen(int width, int height, const terminal::Theme& theme) {
    terminal::Screen screen;
    screen.resize(width, height);
    screen.clear(theme.text);
    return screen;
}

}  // namespace

MDVIEW_TEST(screen, draws_text_and_reports_plain_text) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(10, 3, theme);
    screen.drawText(2, 1, "hi", theme.text);
    // toText() keeps blank rows above the content (they are positional) and
    // only drops the blank rows at the very bottom.
    CHECK_EQ(screen.toText(), std::string("\n  hi"));
    CHECK_EQ(screen.at(2, 1).ch, char32_t{U'h'});
}

MDVIEW_TEST(screen, wide_glyphs_occupy_two_cells) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(10, 1, theme);
    screen.drawText(0, 0, "\xE6\xBC\xA2x", theme.text);
    CHECK_EQ(static_cast<unsigned>(screen.at(0, 0).ch), 0x6F22u);
    CHECK_EQ(static_cast<unsigned>(screen.at(1, 0).ch), static_cast<unsigned>(U' '));
    CHECK_EQ(static_cast<unsigned>(screen.at(2, 0).ch), static_cast<unsigned>(U'x'));
    CHECK_EQ(screen.toText(), std::string("\xE6\xBC\xA2x"));
}

MDVIEW_TEST(screen, wide_glyph_is_replaced_by_a_blank_when_it_does_not_fit) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(1, 1, theme);
    screen.drawText(0, 0, "\xE6\xBC\xA2", theme.text);
    CHECK_EQ(static_cast<unsigned>(screen.at(0, 0).ch), static_cast<unsigned>(U' '));
}

MDVIEW_TEST(screen, overwriting_a_wide_glyph_clears_its_partner) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(6, 1, theme);
    screen.drawText(0, 0, "\xE6\xBC\xA2z", theme.text);
    screen.drawText(1, 0, "X", theme.text);  // clobber the trailing half
    CHECK_EQ(static_cast<unsigned>(screen.at(0, 0).ch), static_cast<unsigned>(U' '));
    CHECK_EQ(static_cast<unsigned>(screen.at(1, 0).ch), static_cast<unsigned>(U'X'));
}

MDVIEW_TEST(screen, clipping_restricts_drawing) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(10, 3, theme);
    screen.pushClip(Rect{2, 1, 4, 1});
    screen.drawText(0, 1, "abcdefgh", theme.text);
    screen.drawText(0, 0, "outside", theme.text);
    screen.popClip();
    CHECK_EQ(screen.toText(), std::string("\n  cdef"));
}

MDVIEW_TEST(screen, box_drawing_ascii) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(6, 3, theme);
    screen.drawBox(Rect{0, 0, 6, 3}, terminal::BorderStyle::Ascii, theme.text);
    CHECK_EQ(screen.toText(), std::string("+----+\n|    |\n+----+"));
    // Nothing is drawn outside the scrollbar-free grid.
    CHECK_EQ(static_cast<unsigned>(screen.at(0, 1).ch), static_cast<unsigned>(U'|'));
}

MDVIEW_TEST(screen, frame_with_title) {
    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen = makeScreen(14, 3, theme);
    screen.drawFrame(Rect{0, 0, 14, 3}, terminal::BorderStyle::Ascii, theme.text, "hi", theme.text);
    const std::string text = screen.toText();
    CHECK_CONTAINS(text, "hi");
    CHECK_EQ(static_cast<unsigned>(screen.at(0, 0).ch), static_cast<unsigned>(U'+'));
    CHECK_EQ(static_cast<unsigned>(screen.at(13, 0).ch), static_cast<unsigned>(U'+'));
}

MDVIEW_TEST(screen, flush_writes_only_changes) {
    terminal::HeadlessTerminal term({20, 4});
    term.initialize();

    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen;
    screen.resize(20, 4);
    screen.clear(theme.text);
    screen.drawText(0, 0, "hello", theme.text);

    screen.flush(term);
    CHECK(term.output().size() > 0);

    term.clearOutput();
    screen.flush(term);
    CHECK_EQ(term.output().size(), std::size_t{0});

    screen.drawText(0, 1, "x", theme.text);
    screen.flush(term);
    CHECK_CONTAINS(term.output(), "x");
    CHECK(term.output().size() < 64);
}

MDVIEW_TEST(screen, resize_forces_a_full_repaint) {
    terminal::HeadlessTerminal term({10, 2});
    term.initialize();

    const terminal::Theme theme = terminal::Theme::plain();
    terminal::Screen screen;
    screen.resize(10, 2);
    screen.clear(theme.text);
    screen.flush(term);

    term.clearOutput();
    screen.resize(20, 4);
    screen.clear(theme.text);
    screen.flush(term);
    CHECK(term.output().size() > 0);
}
