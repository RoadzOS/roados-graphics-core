#include <cmath>

#include "roados/Shape.h"

namespace roados {

Path Circle::toPath() const { return Path().addCircle(cx_, cy_, r_); }

Path Ellipse::toPath() const { return Path().addEllipse({cx_ - rx_, cy_ - ry_, 2 * rx_, 2 * ry_}); }

Path Rectangle::toPath() const { return Path().addRect(r_); }

Path RoundedRect::toPath() const {
    if (rx_ <= 0 && ry_ <= 0) return Path().addRect(r_);
    // SVG rule: a missing radius takes the other's value.
    const float rx = rx_ > 0 ? rx_ : ry_;
    const float ry = ry_ > 0 ? ry_ : rx_;
    return Path().addRoundedRect(r_, rx, ry);
}

Path Square::toPath() const {
    const Rect r{x_, y_, side_, side_};
    return radius_ > 0 ? Path().addRoundedRect(r, radius_, radius_) : Path().addRect(r);
}

Path Triangle::toPath() const { return Path().addPolygon({p_[0], p_[1], p_[2]}, true); }

Polygon Polygon::regular(int sides, float cx, float cy, float radius, float rotationDeg) {
    Polygon p;
    if (sides < 3) return p;
    constexpr double kPi = 3.14159265358979323846;
    for (int i = 0; i < sides; ++i) {
        const double a = (rotationDeg + 360.0 * i / sides) * kPi / 180.0;
        p.add(float(cx + radius * std::cos(a)), float(cy + radius * std::sin(a)));
    }
    return p;
}

Path Polygon::toPath() const { return Path().addPolygon(pts_, true); }

Path Line::toPath() const { return Path().moveTo(a_.x, a_.y).lineTo(b_.x, b_.y); }

Path Polyline::toPath() const { return Path().addPolygon(pts_, false); }

Rect ImageShape::destination() const {
    Rect r = dst_;
    if (r.width <= 0) r.width = float(image_.width());
    if (r.height <= 0) r.height = float(image_.height());
    return r;
}

}  // namespace roados
