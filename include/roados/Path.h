#pragma once
/// Vector path as a value type. Its verbs mirror Skia's exactly (SVG arcs
/// become conic segments), so conversion to and from Skia is lossless.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "roados/Geometry.h"
#include "roados/Result.h"
#include "roados/Transform.h"

namespace roados {

enum class FillRule { NonZero, EvenOdd };

class Path {
public:
    enum class Verb : std::uint8_t { Move, Line, Quad, Conic, Cubic, Close };

    Path() = default;

    /// Parses SVG path data (all commands, absolute and relative, including arcs).
    static Result<Path> fromSvg(std::string_view d);

    Path& moveTo(float x, float y);
    Path& lineTo(float x, float y);
    Path& quadTo(float cx, float cy, float x, float y);
    Path& cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y);
    /// Rational quadratic; weight 1 = quad, <1 = elliptical, >1 = hyperbolic.
    Path& conicTo(float cx, float cy, float x, float y, float weight);
    /// SVG elliptical arc ("A" command) from the current point to (x, y).
    Path& arcTo(float rx, float ry, float xAxisRotationDeg, bool largeArc, bool sweep, float x, float y);
    Path& close();

    Path& addRect(const Rect& r);
    Path& addRoundedRect(const Rect& r, float rx, float ry);
    Path& addEllipse(const Rect& bounds);
    Path& addCircle(float cx, float cy, float radius);
    Path& addPolygon(const std::vector<Point>& points, bool closed);
    Path& addPath(const Path& other, const Transform& transform = {});

    Path& setFillRule(FillRule rule) { fillRule_ = rule; return *this; }
    FillRule fillRule() const { return fillRule_; }

    Path transformed(const Transform& t) const;
    /// Tight bounds of the geometry (curve extrema, not control points).
    Rect bounds() const;
    /// Serialises as absolute SVG path data.
    std::string toSvg() const;

    bool isEmpty() const { return verbs_.empty(); }
    const std::vector<Verb>& verbs() const { return verbs_; }
    const std::vector<Point>& points() const { return points_; }
    const std::vector<float>& conicWeights() const { return weights_; }

    /// Geometric equality: same verbs, points, conic weights and fill rule.
    friend bool operator==(const Path& a, const Path& b) {
        return a.verbs_ == b.verbs_ && a.points_ == b.points_ && a.weights_ == b.weights_ &&
               a.fillRule_ == b.fillRule_;
    }

private:
    void ensureMove();
    Point lastPoint() const;

    std::vector<Verb> verbs_;
    std::vector<Point> points_;
    std::vector<float> weights_;
    FillRule fillRule_ = FillRule::NonZero;
    std::size_t contourStart_ = 0;  // index into points_ of the current contour's move point
};

}  // namespace roados
