#pragma once
/// Plain geometric value types (float, y-down coordinate system as in SVG).

namespace roados {

struct Point {
    float x = 0, y = 0;
    friend constexpr bool operator==(const Point&, const Point&) = default;
};

struct Size {
    float width = 0, height = 0;
    friend constexpr bool operator==(const Size&, const Size&) = default;
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;

    static constexpr Rect fromLTRB(float l, float t, float r, float b) { return {l, t, r - l, b - t}; }
    constexpr float left() const { return x; }
    constexpr float top() const { return y; }
    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr Point center() const { return {x + width / 2, y + height / 2}; }
    constexpr bool isEmpty() const { return !(width > 0 && height > 0); }
    constexpr bool contains(Point p) const { return p.x >= x && p.x < right() && p.y >= y && p.y < bottom(); }
    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

}  // namespace roados
