#include "terminal/style.hpp"

#include <algorithm>

namespace mdview::terminal {

namespace {

struct Rgb8 {
    int r;
    int g;
    int b;
};

// The classic 16 colour palette (xterm defaults).
constexpr Rgb8 kBasicColors[16] = {
    {0, 0, 0},       {205, 0, 0},     {0, 205, 0},     {205, 205, 0},
    {0, 0, 238},     {205, 0, 205},   {0, 205, 205},   {229, 229, 229},
    {127, 127, 127}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
    {92, 92, 255},   {255, 0, 255},   {0, 255, 255},   {255, 255, 255},
};

Rgb8 indexedToRgb(int slot) {
    slot = std::clamp(slot, 0, 255);
    if (slot < 16) return kBasicColors[slot];
    if (slot < 232) {
        const int offset = slot - 16;
        const int r = offset / 36;
        const int g = (offset / 6) % 6;
        const int b = offset % 6;
        const auto channel = [](int level) { return level == 0 ? 0 : 55 + level * 40; };
        return Rgb8{channel(r), channel(g), channel(b)};
    }
    const int level = 8 + (slot - 232) * 10;
    return Rgb8{level, level, level};
}

int nearestBasic(const Rgb8& color) {
    int best = 7;
    long bestDistance = -1;
    for (int i = 0; i < 16; ++i) {
        const long dr = color.r - kBasicColors[i].r;
        const long dg = color.g - kBasicColors[i].g;
        const long db = color.b - kBasicColors[i].b;
        const long distance = dr * dr + dg * dg + db * db;
        if (bestDistance < 0 || distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void appendParam(std::string& out, int value) {
    if (!out.empty() && out.back() != '[') out.push_back(';');
    out.append(std::to_string(value));
}

void appendColor(std::string& out, const Color& color, ColorMode mode, bool foreground) {
    const Color usable = degradeColor(color, mode);
    if (usable.kind == Color::Kind::Default) {
        appendParam(out, foreground ? 39 : 49);
        return;
    }
    if (usable.kind == Color::Kind::Rgb) {
        appendParam(out, foreground ? 38 : 48);
        appendParam(out, 2);
        appendParam(out, usable.r);
        appendParam(out, usable.g);
        appendParam(out, usable.b);
        return;
    }
    const int slot = usable.index;
    if (slot < 8) {
        appendParam(out, (foreground ? 30 : 40) + slot);
    } else if (slot < 16) {
        appendParam(out, (foreground ? 90 : 100) + (slot - 8));
    } else {
        appendParam(out, foreground ? 38 : 48);
        appendParam(out, 5);
        appendParam(out, slot);
    }
}

}  // namespace

int degradeIndexedToBasic(int slot) {
    if (slot >= 0 && slot < 16) return slot;
    return nearestBasic(indexedToRgb(slot));
}

int rgbToIndexed(int r, int g, int b) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);

    const auto toLevel = [](int channel) {
        if (channel < 48) return 0;
        if (channel < 115) return 1;
        return (channel - 35) / 40;
    };
    const int levelR = toLevel(r);
    const int levelG = toLevel(g);
    const int levelB = toLevel(b);
    const int cube = 16 + 36 * levelR + 6 * levelG + levelB;

    // Also consider the grayscale ramp, which is often a better match.
    const int grayAverage = (r + g + b) / 3;
    const int grayLevel = std::clamp((grayAverage - 8) / 10, 0, 23);
    const int gray = 232 + grayLevel;

    const Rgb8 cubeRgb = indexedToRgb(cube);
    const Rgb8 grayRgb = indexedToRgb(gray);
    const long cubeDistance = (r - cubeRgb.r) * (r - cubeRgb.r) + (g - cubeRgb.g) * (g - cubeRgb.g) +
                              (b - cubeRgb.b) * (b - cubeRgb.b);
    const long grayDistance = (r - grayRgb.r) * (r - grayRgb.r) + (g - grayRgb.g) * (g - grayRgb.g) +
                              (b - grayRgb.b) * (b - grayRgb.b);
    return cubeDistance <= grayDistance ? cube : gray;
}

Color degradeColor(const Color& color, ColorMode mode) {
    if (mode == ColorMode::None) return Color::def();
    switch (color.kind) {
        case Color::Kind::Default:
            return color;
        case Color::Kind::Indexed:
            if (mode == ColorMode::Basic && color.index >= 16) {
                return Color::basic(degradeIndexedToBasic(color.index));
            }
            return color;
        case Color::Kind::Rgb:
            if (mode == ColorMode::TrueColor) return color;
            if (mode == ColorMode::Ansi256) {
                return Color::indexed(rgbToIndexed(color.r, color.g, color.b));
            }
            return Color::basic(nearestBasic(Rgb8{color.r, color.g, color.b}));
    }
    return Color::def();
}

std::string sgrFor(const Style& style, ColorMode mode) {
    if (mode == ColorMode::None) return {};

    std::string params = "\x1b[";
    appendParam(params, 0);  // reset first: styles never bleed into each other
    if (style.bold) appendParam(params, 1);
    if (style.dim) appendParam(params, 2);
    if (style.italic) appendParam(params, 3);
    if (style.underline) appendParam(params, 4);
    if (style.reverse) appendParam(params, 7);
    if (style.strikethrough) appendParam(params, 9);

    const Color fg = degradeColor(style.fg, mode);
    if (fg.kind != Color::Kind::Default) appendColor(params, fg, mode, true);
    const Color bg = degradeColor(style.bg, mode);
    if (bg.kind != Color::Kind::Default) appendColor(params, bg, mode, false);

    params.push_back('m');
    return params;
}

Style withoutBackground(const Style& style) {
    Style copy = style;
    copy.bg = Color::def();
    return copy;
}

Style highlightFor(const Style& style, bool current) {
    // A highlight must stay readable on top of any span it covers, so it keeps
    // the underlying foreground, drops attributes that fight the overlay, and
    // signals itself through the background -- or through reverse video when
    // the terminal cannot show one.
    Style overlay;
    overlay.fg = style.fg;
    overlay.bg = style.bg;
    overlay.underline = style.underline;
    overlay.bold = current ? true : style.bold;
    if (current) {
        overlay.reverse = style.bg.kind == Color::Kind::Default;
    }
    return overlay;
}

namespace {

Style makeStyle(Color fg, bool bold = false, bool italic = false) {
    Style style;
    style.fg = fg;
    style.bold = bold;
    style.italic = italic;
    return style;
}

Style onSurface(Color fg, Color bg, bool bold = false) {
    Style style;
    style.fg = fg;
    style.bg = bg;
    style.bold = bold;
    return style;
}

}  // namespace

Theme Theme::plain() {
    // Everything falls back to the terminal's default foreground and
    // background; hierarchy is expressed with attributes only.
    Theme theme;
    theme.text = Style{};
    theme.muted = Style{};
    theme.muted.dim = true;
    theme.accent = Style{};

    for (int level = 0; level < 6; ++level) {
        theme.heading[static_cast<std::size_t>(level)] = Style{}.withBold();
    }
    theme.heading[0].underline = true;
    theme.heading[1].underline = true;

    theme.inlineCode = Style{};
    theme.inlineCode.reverse = true;
    theme.codeBlock = Style{};
    theme.codeLanguage = Style{}.withUnderline();
    theme.codeBorder = Style{};
    theme.codeBorder.dim = true;

    // Without colour, syntax highlighting falls back to attributes so the
    // structure is still legible on a monochrome terminal.
    theme.code.plain = Style{};
    theme.code.keyword = Style{}.withBold();
    theme.code.type = Style{}.withBold();
    theme.code.string = Style{};
    theme.code.number = Style{};
    theme.code.comment = Style{};
    theme.code.comment.italic = true;
    theme.code.comment.dim = true;
    theme.code.function = Style{};
    theme.code.operatorSymbol = Style{};
    theme.code.punctuation = Style{};
    theme.code.preprocessor = Style{}.withUnderline();
    theme.code.constant = Style{};

    theme.link = Style{}.withUnderline();
    theme.linkUrl = Style{};
    theme.linkUrl.dim = true;
    theme.image = Style{};

    theme.bullet = Style{};
    theme.listMarker = Style{}.withBold();

    theme.quoteBar = Style{}.withBold();
    theme.quoteText = Style{};
    theme.quoteText.italic = true;

    theme.rule = Style{};
    theme.rule.dim = true;
    theme.tableBorder = Style{};
    theme.tableHeader = Style{}.withBold();
    theme.tableAlignMarker = Style{};

    theme.checkbox = Style{};
    theme.checkboxChecked = Style{}.withBold();

    theme.treeDirectory = Style{}.withBold();
    theme.treeFile = Style{};
    theme.treeMarkdown = Style{}.withBold();
    theme.treeGuide = Style{};
    theme.treeGuide.dim = true;
    theme.treeSelected = Style{};
    theme.treeSelected.reverse = true;
    theme.treeSelectedMuted = Style{};
    theme.treeSelectedMuted.reverse = true;
    theme.treeSelectedMuted.dim = true;

    theme.headerBar = Style{};
    theme.headerTitle = makeStyle(Color::def(), true).withUnderline();
    theme.headerPath = Style{};
    theme.headerPath.dim = true;

    theme.statusBar = Style{};
    theme.statusText = Style{};
    theme.statusKey = makeStyle(Color::def(), true).withUnderline();

    theme.border = Style{};
    theme.borderFocused = Style{}.withBold();
    theme.scrollbar = Style{};
    theme.scrollbar.dim = true;
    theme.scrollbarThumb = Style{}.withBold();

    theme.helpText = Style{};
    theme.helpKey = makeStyle(Color::def(), true).withUnderline();
    theme.helpTitle = makeStyle(Color::def(), true).withUnderline();

    theme.error = Style{}.withBold();
    theme.warning = Style{}.withBold();

    theme.searchMatch = highlightFor(theme.text, false);
    theme.searchMatchCurrent = highlightFor(theme.text, true);
    return theme;
}

Theme Theme::colored(ColorMode mode) {
    const auto fg = [mode](Color color) {
        Style style;
        style.fg = degradeColor(color, mode);
        return style;
    };
    const auto surface = [mode](Color foreground, Color background) {
        Style style;
        style.fg = degradeColor(foreground, mode);
        style.bg = degradeColor(background, mode);
        return style;
    };

    const Color kText = Color::def();
    const Color kMuted = Color::basic(8);    // dark grey
    const Color kAccent = Color::basic(6);   // cyan
    const Color kCyan = Color::basic(14);    // bright cyan
    const Color kBlue = Color::basic(12);    // bright blue
    const Color kGreen = Color::basic(10);   // bright green
    const Color kYellow = Color::basic(11);  // bright yellow
    const Color kMagenta = Color::basic(13);
    const Color kRed = Color::basic(9);
    const Color kWhite = Color::basic(15);
    const Color kChrome = Color::indexed(235);
    const Color kChromeText = Color::indexed(252);
    const Color kCodeSurface = Color::indexed(236);
    const Color kCodeText = Color::indexed(252);
    const Color kSelection = Color::indexed(238);

    Theme theme;
    theme.text = fg(kText);
    theme.muted = fg(kMuted);
    theme.accent = fg(kAccent);

    theme.heading[0] = fg(kCyan).withBold();
    theme.heading[0].underline = true;
    theme.heading[1] = fg(kBlue).withBold();
    theme.heading[1].underline = true;
    theme.heading[2] = fg(kGreen).withBold();
    theme.heading[3] = fg(kYellow).withBold();
    theme.heading[4] = fg(kMagenta).withBold();
    theme.heading[5] = fg(kRed).withBold();

    theme.inlineCode = surface(kYellow, kCodeSurface);
    theme.codeBlock = surface(kCodeText, kCodeSurface);
    theme.codeLanguage = surface(kMuted, kCodeSurface);
    theme.codeBorder = surface(kMuted, kCodeSurface);

    theme.code.plain = surface(kCodeText, kCodeSurface);
    theme.code.keyword = surface(kMagenta, kCodeSurface);
    theme.code.keyword.bold = true;
    theme.code.type = surface(kCyan, kCodeSurface);
    theme.code.string = surface(kGreen, kCodeSurface);
    theme.code.number = surface(kYellow, kCodeSurface);
    theme.code.comment = surface(kMuted, kCodeSurface);
    theme.code.comment.italic = true;
    theme.code.function = surface(kBlue, kCodeSurface);
    theme.code.operatorSymbol = surface(kCyan, kCodeSurface);
    theme.code.punctuation = surface(kCodeText, kCodeSurface);
    theme.code.preprocessor = surface(kMagenta, kCodeSurface);
    theme.code.constant = surface(kYellow, kCodeSurface);

    theme.link = fg(kBlue).withUnderline();
    theme.linkUrl = fg(kMuted);
    theme.image = fg(kMagenta);

    theme.bullet = fg(kAccent);
    theme.listMarker = fg(kCyan).withBold();

    theme.quoteBar = fg(kAccent);
    theme.quoteText = fg(kMuted);
    theme.quoteText.italic = true;

    theme.rule = fg(kMuted);
    theme.tableBorder = fg(kMuted);
    theme.tableHeader = fg(kWhite).withBold();
    theme.tableAlignMarker = fg(kMuted);

    theme.checkbox = fg(kMuted);
    theme.checkboxChecked = fg(kGreen).withBold();

    theme.treeDirectory = fg(kBlue).withBold();
    theme.treeFile = fg(kText);
    theme.treeMarkdown = fg(kCyan);
    theme.treeGuide = fg(kMuted);
    theme.treeSelected = surface(kWhite, kSelection);
    theme.treeSelected.bold = true;
    theme.treeSelectedMuted = surface(kChromeText, kSelection);

    theme.headerBar = surface(kChromeText, kChrome);
    theme.headerTitle = surface(kCyan, kChrome);
    theme.headerTitle.bold = true;
    theme.headerPath = surface(kMuted, kChrome);

    theme.statusBar = surface(kChromeText, kChrome);
    theme.statusText = surface(kChromeText, kChrome);
    theme.statusKey = surface(kCyan, kChrome);
    theme.statusKey.bold = true;

    theme.border = fg(kMuted);
    theme.borderFocused = fg(kAccent);
    theme.scrollbar = fg(kMuted);
    theme.scrollbarThumb = fg(kAccent);

    // The help panel is a modal, so it gets its own opaque surface: every
    // style used inside it carries the same background.
    const Color kPanelSurface = Color::indexed(235);
    theme.helpText = surface(kChromeText, kPanelSurface);
    theme.helpKey = surface(kCyan, kPanelSurface);
    theme.helpKey.bold = true;
    theme.helpTitle = surface(kYellow, kPanelSurface);
    theme.helpTitle.bold = true;

    theme.error = fg(kRed).withBold();
    theme.warning = fg(kYellow).withBold();

    const Color kMatchBg = Color::indexed(136);      // muted amber
    const Color kMatchCurrentBg = Color::indexed(214);  // bright amber
    theme.searchMatch = surface(kText, kMatchBg);
    theme.searchMatchCurrent = surface(Color::indexed(16), kMatchCurrentBg);
    theme.searchMatchCurrent.bold = true;
    return theme;
}

}  // namespace mdview::terminal
