#include "roados/Path.h"

#include <charconv>
#include <string>
#include <system_error>

#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/utils/SkParsePath.h"
#include "renderer/SkiaConvert.h"

namespace roados {

Point Path::lastPoint() const {
    if (verbs_.empty()) return {0, 0};
    if (verbs_.back() == Verb::Close) return points_[contourStart_];
    return points_.back();
}

// SVG semantics: drawing without a current point starts at (0,0); drawing after
// close() starts a new contour at the closed contour's start point.
void Path::ensureMove() {
    if (verbs_.empty()) moveTo(0, 0);
    else if (verbs_.back() == Verb::Close) {
        const Point p = points_[contourStart_];
        moveTo(p.x, p.y);
    }
}

Path& Path::moveTo(float x, float y) {
    if (!verbs_.empty() && verbs_.back() == Verb::Move) {
        points_.back() = {x, y};  // consecutive moves collapse, as in Skia
    } else {
        verbs_.push_back(Verb::Move);
        points_.push_back({x, y});
    }
    contourStart_ = points_.size() - 1;
    return *this;
}

Path& Path::lineTo(float x, float y) {
    ensureMove();
    verbs_.push_back(Verb::Line);
    points_.push_back({x, y});
    return *this;
}

Path& Path::quadTo(float cx, float cy, float x, float y) {
    ensureMove();
    verbs_.push_back(Verb::Quad);
    points_.insert(points_.end(), {{cx, cy}, {x, y}});
    return *this;
}

Path& Path::conicTo(float cx, float cy, float x, float y, float weight) {
    ensureMove();
    verbs_.push_back(Verb::Conic);
    points_.insert(points_.end(), {{cx, cy}, {x, y}});
    weights_.push_back(weight);
    return *this;
}

Path& Path::cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    ensureMove();
    verbs_.push_back(Verb::Cubic);
    points_.insert(points_.end(), {{c1x, c1y}, {c2x, c2y}, {x, y}});
    return *this;
}

Path& Path::close() {
    if (!verbs_.empty() && verbs_.back() != Verb::Close) verbs_.push_back(Verb::Close);
    return *this;
}

// Appends every verb of `sp`; if `skipLeadingMove`, the first move is dropped so
// the geometry continues the current contour.
static void append(Path& dst, const SkPath& sp, bool skipLeadingMove) {
    Path tail = sk::fromSkPath(sp);
    const auto& v = tail.verbs();
    const auto& p = tail.points();
    const auto& w = tail.conicWeights();
    std::size_t pi = 0, wi = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        switch (v[i]) {
            case Path::Verb::Move:
                if (!(skipLeadingMove && i == 0)) dst.moveTo(p[pi].x, p[pi].y);
                pi += 1;
                break;
            case Path::Verb::Line: dst.lineTo(p[pi].x, p[pi].y); pi += 1; break;
            case Path::Verb::Quad: dst.quadTo(p[pi].x, p[pi].y, p[pi + 1].x, p[pi + 1].y); pi += 2; break;
            case Path::Verb::Conic:
                dst.conicTo(p[pi].x, p[pi].y, p[pi + 1].x, p[pi + 1].y, w[wi++]);
                pi += 2;
                break;
            case Path::Verb::Cubic:
                dst.cubicTo(p[pi].x, p[pi].y, p[pi + 1].x, p[pi + 1].y, p[pi + 2].x, p[pi + 2].y);
                pi += 3;
                break;
            case Path::Verb::Close: dst.close(); break;
        }
    }
}

Path& Path::arcTo(float rx, float ry, float xAxisRotationDeg, bool largeArc, bool sweep, float x, float y) {
    ensureMove();
    const Point start = lastPoint();
    SkPathBuilder b;
    b.moveTo(start.x, start.y);
    // SVG sweep-flag=1 is the positive-angle direction, i.e. clockwise in y-down space.
    b.arcTo({rx, ry}, xAxisRotationDeg, largeArc ? SkPathBuilder::kLarge_ArcSize : SkPathBuilder::kSmall_ArcSize,
            sweep ? SkPathDirection::kCW : SkPathDirection::kCCW, {x, y});
    append(*this, b.detach(), /*skipLeadingMove=*/true);
    return *this;
}

Path& Path::addRect(const Rect& r) {
    append(*this, SkPath::Rect(sk::rect(r)), false);
    return *this;
}

Path& Path::addRoundedRect(const Rect& r, float rx, float ry) {
    append(*this, SkPath::RRect(SkRRect::MakeRectXY(sk::rect(r), rx, ry)), false);
    return *this;
}

Path& Path::addEllipse(const Rect& bounds) {
    append(*this, SkPath::Oval(sk::rect(bounds)), false);
    return *this;
}

Path& Path::addCircle(float cx, float cy, float radius) {
    return addEllipse({cx - radius, cy - radius, 2 * radius, 2 * radius});
}

Path& Path::addPolygon(const std::vector<Point>& pts, bool closed) {
    if (pts.empty()) return *this;
    moveTo(pts[0].x, pts[0].y);
    for (std::size_t i = 1; i < pts.size(); ++i) lineTo(pts[i].x, pts[i].y);
    if (closed) close();
    return *this;
}

Path& Path::addPath(const Path& other, const Transform& t) {
    append(*this, sk::path(t.isIdentity() ? other : other.transformed(t)), false);
    return *this;
}

Path Path::transformed(const Transform& t) const {
    Path out = *this;
    for (auto& p : out.points_) p = t.map(p);
    return out;
}

Rect Path::bounds() const { return sk::rect(sk::path(*this).computeTightBounds()); }

// Serialises exactly the stored verbs (Skia's ToSVGString would add an explicit
// closing segment before Z). Numbers use the shortest round-trip form.
std::string Path::toSvg() const {
    std::string out;
    char buf[32];
    auto num = [&](float v) {
        if (v == 0) v = 0;  // normalise -0
        auto [end, ec] = std::to_chars(buf, buf + sizeof buf, v);
        if (ec == std::errc()) out.append(buf, end);
    };
    auto pt = [&](Point p) { num(p.x); out += ' '; num(p.y); };
    std::size_t pi = 0, wi = 0;
    Point current{0, 0};
    for (Verb v : verbs_) {
        switch (v) {
            case Verb::Move: out += 'M'; pt(points_[pi]); current = points_[pi]; pi += 1; break;
            case Verb::Line: out += 'L'; pt(points_[pi]); current = points_[pi]; pi += 1; break;
            case Verb::Quad:
                out += 'Q'; pt(points_[pi]); out += ' '; pt(points_[pi + 1]);
                current = points_[pi + 1];
                pi += 2;
                break;
            case Verb::Conic: {
                // SVG has no conics: approximate with quads (2^3 segments, sub-pixel error at icon sizes).
                SkPoint quads[1 + 2 * 8];
                const int n = SkPath::ConvertConicToQuads({current.x, current.y}, {points_[pi].x, points_[pi].y},
                                                          {points_[pi + 1].x, points_[pi + 1].y}, weights_[wi++],
                                                          quads, 3);
                for (int q = 0; q < n; ++q) {
                    out += 'Q'; pt({quads[1 + 2 * q].fX, quads[1 + 2 * q].fY});
                    out += ' '; pt({quads[2 + 2 * q].fX, quads[2 + 2 * q].fY});
                }
                current = points_[pi + 1];
                pi += 2;
                break;
            }
            case Verb::Cubic:
                out += 'C'; pt(points_[pi]); out += ' '; pt(points_[pi + 1]); out += ' '; pt(points_[pi + 2]);
                current = points_[pi + 2];
                pi += 3;
                break;
            case Verb::Close: out += 'Z'; break;
        }
    }
    return out;
}

Result<Path> Path::fromSvg(std::string_view d) {
    const std::string z(d);  // SkParsePath requires a NUL-terminated string
    SkPath sp;
    if (!SkParsePath::FromSVGString(z.c_str(), &sp))
        return makeError(ErrorCode::Parse, "invalid SVG path data: '" + z.substr(0, 80) + "'");
    return sk::fromSkPath(sp);
}

}  // namespace roados
