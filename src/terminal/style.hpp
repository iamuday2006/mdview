#pragma once

#include <array>
#include <cstdint>
#include <string>

// Presentation primitives shared by the screen buffer, the Markdown renderer
// and the UI widgets.  A Style is a *description* of how text should look, not
// a platform detail: turning it into bytes is the job of sgrFor() below, which
// degrades gracefully when the terminal supports fewer colours.

namespace mdview::terminal {

/// How much colour the terminal can be trusted to render.
enum class ColorMode : std::uint8_t {
    None = 0,   ///< No SGR colour at all (monochrome terminals, --no-color).
    Basic,      ///< The classic 8 + 8 bright ANSI colours.
    Ansi256,    ///< xterm 256 colour palette.
    TrueColor,  ///< 24-bit RGB.
};

struct Color {
    enum class Kind : std::uint8_t { Default, Indexed, Rgb };

    Kind kind = Kind::Default;
    std::uint8_t index = 0;  ///< palette slot for Kind::Indexed (0-255)
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    static constexpr Color def() noexcept { return Color{}; }

    /// Palette slot 0-15 selects one of the classic ANSI colours.
    static constexpr Color basic(int slot) noexcept {
        return Color{Kind::Indexed, static_cast<std::uint8_t>(slot), 0, 0, 0};
    }

    static constexpr Color indexed(int slot) noexcept {
        return Color{Kind::Indexed, static_cast<std::uint8_t>(slot), 0, 0, 0};
    }

    static constexpr Color rgb(int red, int green, int blue) noexcept {
        return Color{Kind::Rgb, 0, static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green),
                     static_cast<std::uint8_t>(blue)};
    }

    friend bool operator==(const Color&, const Color&) noexcept = default;
};

struct Style {
    Color fg;
    Color bg;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
    bool strikethrough = false;

    friend bool operator==(const Style&, const Style&) noexcept = default;
    friend bool operator!=(const Style& a, const Style& b) noexcept { return !(a == b); }

    /// Convenience for deriving variants (e.g. heading colour + bold).
    Style withBold(bool value = true) const {
        Style copy = *this;
        copy.bold = value;
        return copy;
    }
    Style withUnderline(bool value = true) const {
        Style copy = *this;
        copy.underline = value;
        return copy;
    }
};

/// Maps a palette slot from the 256 colour cube down to one of the 16 classic
/// colours, so themes built for modern terminals still look sane on a basic
/// console.
int degradeIndexedToBasic(int slot);

/// Nearest xterm-256 slot for an RGB triple.
int rgbToIndexed(int r, int g, int b);

/// Rewrites `color` so that it can be expressed with `mode`.
Color degradeColor(const Color& color, ColorMode mode);

/// Full SGR escape sequence for a style, or an empty string when the terminal
/// has no colour support.  Always starts by resetting, so styles never bleed.
std::string sgrFor(const Style& style, ColorMode mode);

/// Fallback used when a CodeView or TableView needs a fill colour that the
/// terminal cannot express: returns `style` without the background.
Style withoutBackground(const Style& style);

/// Derives a variant of `style` that reads as a selection overlay on top of
/// already-styled text: keeps the original foreground, replaces the background
/// when the mode can show one, and reverses otherwise.
Style highlightFor(const Style& style, bool current);

/// Colours for syntax highlighting, one per token class.
struct CodeColors {
    Style plain;
    Style keyword;
    Style type;
    Style string;
    Style number;
    Style comment;
    Style function;
    Style operatorSymbol;
    Style punctuation;
    Style preprocessor;
    Style constant;
};

/// The palette used by the whole application.  Each field names a role rather
/// than a colour, so swapping the theme never touches rendering code.
struct Theme {
    Style text;
    Style muted;
    Style accent;

    std::array<Style, 6> heading{};

    Style inlineCode;
    Style codeBlock;
    Style codeLanguage;
    Style codeBorder;
    CodeColors code;

    Style link;
    Style linkUrl;
    Style image;

    Style bullet;
    Style listMarker;

    Style quoteBar;
    Style quoteText;

    Style rule;
    Style tableBorder;
    Style tableHeader;
    Style tableAlignMarker;

    Style checkbox;
    Style checkboxChecked;

    Style treeDirectory;
    Style treeFile;
    Style treeMarkdown;
    Style treeGuide;
    Style treeSelected;
    Style treeSelectedMuted;

    Style headerBar;
    Style headerTitle;
    Style headerPath;

    Style statusBar;
    Style statusText;
    Style statusKey;

    Style border;
    Style borderFocused;
    Style scrollbar;
    Style scrollbarThumb;

    Style helpText;
    Style helpKey;
    Style helpTitle;

    Style error;
    Style warning;

    /// Occurrences of the active search query.
    Style searchMatch;
    /// The occurrence the cursor is currently on.
    Style searchMatchCurrent;

    /// A colourful theme; pass a ColorMode so backgrounds can degrade.
    static Theme colored(ColorMode mode);
    /// Monochrome theme used for --no-color, dumb terminals and tests.
    static Theme plain();
};

}  // namespace mdview::terminal
