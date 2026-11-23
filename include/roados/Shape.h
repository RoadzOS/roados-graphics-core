#pragma once
/// Drawable primitives with a fluent builder API.
///
///     Circle().center(120, 120).radius(50).fill(Color("#2196F3"))
///
/// Defaults follow SVG. A closed shape with neither fill nor stroke is
/// filled black. Line and Polyline default to a 1px black stroke.

#include <string>
#include <utility>
#include <vector>

#include "roados/Geometry.h"
#include "roados/Image.h"
#include "roados/Paint.h"
#include "roados/Path.h"
#include "roados/Transform.h"

namespace roados {

/// Shared styling and transform setters (CRTP so chaining keeps the concrete type).
template <class Derived>
class Styled {
public:
    Derived& fill(Brush brush) { style_.fill = std::move(brush); return self(); }
    Derived& noFill() { style_.fill.reset(); explicitNoFill_ = true; return self(); }
    Derived& stroke(Brush brush, float width = 1) {
        Stroke s = style_.stroke.value_or(Stroke{});
        s.brush = std::move(brush);
        s.width = width;
        style_.stroke = std::move(s);
        return self();
    }
    Derived& stroke(Stroke s) { style_.stroke = std::move(s); return self(); }
    Derived& noStroke() { style_.stroke.reset(); return self(); }
    Derived& lineCap(LineCap cap) { strokeRef().cap = cap; return self(); }
    Derived& lineJoin(LineJoin join) { strokeRef().join = join; return self(); }
    Derived& miterLimit(float limit) { strokeRef().miterLimit = limit; return self(); }
    Derived& dash(std::vector<float> intervals, float offset = 0) {
        strokeRef().dash = std::move(intervals);
        strokeRef().dashOffset = offset;
        return self();
    }
    Derived& opacity(float o) { style_.opacity = o; return self(); }
    Derived& blend(BlendMode mode) { style_.blend = mode; return self(); }
    Derived& antiAlias(bool on) { style_.antiAlias = on; return self(); }
    Derived& transform(const Transform& t) { transform_ = t; return self(); }

    const Style& style() const { return style_; }
    const Transform& transform() const { return transform_; }
    /// True when noFill() was requested explicitly (suppresses the default black fill).
    bool fillDisabled() const { return explicitNoFill_; }

protected:
    Style style_;
    Transform transform_;
    bool explicitNoFill_ = false;

private:
    Derived& self() { return static_cast<Derived&>(*this); }
    Stroke& strokeRef() {
        if (!style_.stroke) style_.stroke = Stroke{};
        return *style_.stroke;
    }
};

class Circle : public Styled<Circle> {
public:
    Circle& center(float x, float y) { cx_ = x; cy_ = y; return *this; }
    Circle& radius(float r) { r_ = r; return *this; }
    Point center() const { return {cx_, cy_}; }
    float radius() const { return r_; }
    Path toPath() const;
private:
    float cx_ = 0, cy_ = 0, r_ = 0;
};

class Ellipse : public Styled<Ellipse> {
public:
    Ellipse& center(float x, float y) { cx_ = x; cy_ = y; return *this; }
    Ellipse& radii(float rx, float ry) { rx_ = rx; ry_ = ry; return *this; }
    Path toPath() const;
private:
    float cx_ = 0, cy_ = 0, rx_ = 0, ry_ = 0;
};

class Rectangle : public Styled<Rectangle> {
public:
    Rectangle& position(float x, float y) { r_.x = x; r_.y = y; return *this; }
    Rectangle& size(float w, float h) { r_.width = w; r_.height = h; return *this; }
    Rectangle& rect(const Rect& r) { r_ = r; return *this; }
    const Rect& rect() const { return r_; }
    Path toPath() const;
private:
    Rect r_;
};

class RoundedRect : public Styled<RoundedRect> {
public:
    RoundedRect& position(float x, float y) { r_.x = x; r_.y = y; return *this; }
    RoundedRect& size(float w, float h) { r_.width = w; r_.height = h; return *this; }
    RoundedRect& rect(const Rect& r) { r_ = r; return *this; }
    RoundedRect& radius(float r) { rx_ = ry_ = r; return *this; }
    RoundedRect& radii(float rx, float ry) { rx_ = rx; ry_ = ry; return *this; }
    const Rect& rect() const { return r_; }
    Path toPath() const;
private:
    Rect r_;
    float rx_ = 0, ry_ = 0;
};

class Square : public Styled<Square> {
public:
    Square& position(float x, float y) { x_ = x; y_ = y; return *this; }
    Square& size(float side) { side_ = side; return *this; }
    Square& radius(float r) { radius_ = r; return *this; }
    Path toPath() const;
private:
    float x_ = 0, y_ = 0, side_ = 0, radius_ = 0;
};

class Triangle : public Styled<Triangle> {
public:
    Triangle& points(Point a, Point b, Point c) { p_[0] = a; p_[1] = b; p_[2] = c; return *this; }
    Path toPath() const;
private:
    Point p_[3];
};

class Polygon : public Styled<Polygon> {
public:
    Polygon& points(std::vector<Point> pts) { pts_ = std::move(pts); return *this; }
    Polygon& add(float x, float y) { pts_.push_back({x, y}); return *this; }
    /// Regular n-gon (first vertex at angle `rotationDeg`, clockwise from +x).
    static Polygon regular(int sides, float cx, float cy, float radius, float rotationDeg = -90);
    const std::vector<Point>& points() const { return pts_; }
    Path toPath() const;
private:
    std::vector<Point> pts_;
};

class Line : public Styled<Line> {
public:
    Line& from(float x, float y) { a_ = {x, y}; return *this; }
    Line& to(float x, float y) { b_ = {x, y}; return *this; }
    Path toPath() const;
private:
    Point a_, b_;
};

class Polyline : public Styled<Polyline> {
public:
    Polyline& points(std::vector<Point> pts) { pts_ = std::move(pts); return *this; }
    Polyline& add(float x, float y) { pts_.push_back({x, y}); return *this; }
    Path toPath() const;
private:
    std::vector<Point> pts_;
};

/// Arbitrary vector path (custom geometry, Bézier curves, SVG path data).
class PathShape : public Styled<PathShape> {
public:
    PathShape() = default;
    explicit PathShape(Path p) : path_(std::move(p)) {}
    PathShape& path(Path p) { path_ = std::move(p); return *this; }
    const Path& path() const { return path_; }
    Path toPath() const { return path_; }
private:
    Path path_;
};

/// A raster image placed into a destination rectangle.
class ImageShape : public Styled<ImageShape> {
public:
    ImageShape& image(Image img) { image_ = std::move(img); return *this; }
    ImageShape& position(float x, float y) { dst_.x = x; dst_.y = y; return *this; }
    /// Destination size; 0 means "use the image's own size".
    ImageShape& size(float w, float h) { dst_.width = w; dst_.height = h; return *this; }
    ImageShape& smooth(bool on) { smooth_ = on; return *this; }
    const Image& image() const { return image_; }
    Rect destination() const;
    bool smooth() const { return smooth_; }
private:
    Image image_;
    Rect dst_;
    bool smooth_ = true;
};

}  // namespace roados
