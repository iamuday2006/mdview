#pragma once

// Small, dependency-free rectangle helper shared by the layout code and the
// screen buffer.  Nothing here is platform specific: every coordinate is a
// cell position inside the terminal grid, with (0, 0) at the top-left corner.

namespace mdview {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr int right() const noexcept { return x + width; }
    constexpr int bottom() const noexcept { return y + height; }

    constexpr bool empty() const noexcept { return width <= 0 || height <= 0; }

    constexpr bool contains(int px, int py) const noexcept {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    /// Overlap of two rectangles; the result is empty when they do not overlap.
    constexpr Rect intersected(const Rect& other) const noexcept {
        const int nx = x > other.x ? x : other.x;
        const int ny = y > other.y ? y : other.y;
        const int nr = right() < other.right() ? right() : other.right();
        const int nb = bottom() < other.bottom() ? bottom() : other.bottom();
        return Rect{nx, ny, nr > nx ? nr - nx : 0, nb > ny ? nb - ny : 0};
    }

    constexpr Rect inset(int dx, int dy) const noexcept {
        return Rect{x + dx, y + dy, width - 2 * dx, height - 2 * dy};
    }

    constexpr Rect inset(int amount) const noexcept { return inset(amount, amount); }

    constexpr Rect translated(int dx, int dy) const noexcept {
        return Rect{x + dx, y + dy, width, height};
    }

    constexpr Rect resized(int w, int h) const noexcept { return Rect{x, y, w, h}; }

    friend constexpr bool operator==(const Rect& a, const Rect& b) noexcept {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }
    friend constexpr bool operator!=(const Rect& a, const Rect& b) noexcept { return !(a == b); }
};

/// Carving helpers: they shrink `area` and return the strip that was removed.
/// They are the building blocks of the frame layout (header / tree / content /
/// status bar) and never produce a negative width or height.
inline Rect carveLeft(Rect& area, int amount) {
    if (amount < 0) amount = 0;
    if (amount > area.width) amount = area.width;
    const Rect strip{area.x, area.y, amount, area.height};
    area.x += amount;
    area.width -= amount;
    return strip;
}

inline Rect carveRight(Rect& area, int amount) {
    if (amount < 0) amount = 0;
    if (amount > area.width) amount = area.width;
    area.width -= amount;
    return Rect{area.x + area.width, area.y, amount, area.height};
}

inline Rect carveTop(Rect& area, int amount) {
    if (amount < 0) amount = 0;
    if (amount > area.height) amount = area.height;
    const Rect strip{area.x, area.y, area.width, amount};
    area.y += amount;
    area.height -= amount;
    return strip;
}

inline Rect carveBottom(Rect& area, int amount) {
    if (amount < 0) amount = 0;
    if (amount > area.height) amount = area.height;
    area.height -= amount;
    return Rect{area.x, area.y + area.height, area.width, amount};
}

}  // namespace mdview
