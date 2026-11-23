#pragma once
/// 2D affine transform, using the SVG matrix(a b c d e f) convention:
///
///     | a c e |      x' = a*x + c*y + e
///     | b d f |      y' = b*x + d*y + f
///     | 0 0 1 |
///
/// Static factories (Translate, Rotate, ...) create a transform. Chained
/// lower-case calls compose like an SVG transform list or successive canvas
/// calls: each call applies in the coordinate system set up by the previous
/// ones, so Transform::Translate(10, 0).rotate(90) rotates first, then translates.

#include <optional>

#include "roados/Geometry.h"

namespace roados {

class Transform {
public:
    constexpr Transform() = default;
    constexpr Transform(float a, float b, float c, float d, float e, float f)
        : a_(a), b_(b), c_(c), d_(d), e_(e), f_(f) {}

    static constexpr Transform Identity() { return {}; }
    static constexpr Transform Translate(float dx, float dy) { return {1, 0, 0, 1, dx, dy}; }
    static constexpr Transform Scale(float s) { return {s, 0, 0, s, 0, 0}; }
    static constexpr Transform Scale(float sx, float sy) { return {sx, 0, 0, sy, 0, 0}; }
    /// Clockwise rotation in degrees (y axis points down, as in SVG).
    static Transform Rotate(float degrees);
    static Transform Rotate(float degrees, float cx, float cy);
    static Transform SkewX(float degrees);
    static Transform SkewY(float degrees);
    static constexpr Transform Matrix(float a, float b, float c, float d, float e, float f) {
        return {a, b, c, d, e, f};
    }

    /// this × rhs: rhs is applied to points first.
    constexpr Transform operator*(const Transform& r) const {
        return {a_ * r.a_ + c_ * r.b_, b_ * r.a_ + d_ * r.b_,
                a_ * r.c_ + c_ * r.d_, b_ * r.c_ + d_ * r.d_,
                a_ * r.e_ + c_ * r.f_ + e_, b_ * r.e_ + d_ * r.f_ + f_};
    }

    constexpr Transform translate(float dx, float dy) const { return *this * Translate(dx, dy); }
    constexpr Transform scale(float s) const { return *this * Scale(s); }
    constexpr Transform scale(float sx, float sy) const { return *this * Scale(sx, sy); }
    Transform rotate(float degrees) const { return *this * Rotate(degrees); }
    Transform rotate(float degrees, float cx, float cy) const { return *this * Rotate(degrees, cx, cy); }
    constexpr Transform concat(const Transform& t) const { return *this * t; }

    constexpr Point map(Point p) const { return {a_ * p.x + c_ * p.y + e_, b_ * p.x + d_ * p.y + f_}; }
    /// Axis-aligned bounds of the transformed rectangle.
    Rect mapRect(const Rect& r) const;
    std::optional<Transform> inverted() const;

    constexpr bool isIdentity() const { return *this == Transform(); }
    constexpr float a() const { return a_; }
    constexpr float b() const { return b_; }
    constexpr float c() const { return c_; }
    constexpr float d() const { return d_; }
    constexpr float e() const { return e_; }
    constexpr float f() const { return f_; }

    friend constexpr bool operator==(const Transform&, const Transform&) = default;

private:
    float a_ = 1, b_ = 0, c_ = 0, d_ = 1, e_ = 0, f_ = 0;
};

}  // namespace roados
