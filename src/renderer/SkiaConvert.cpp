#include "renderer/SkiaConvert.h"

#include <type_traits>

#include "include/core/SkPathTypes.h"
#include "include/core/SkPoint.h"

namespace roados::sk {

static_assert(sizeof(Point) == sizeof(SkPoint) && alignof(Point) == alignof(SkPoint) &&
                  std::is_standard_layout_v<Point>,
              "roados::Point must be layout-compatible with SkPoint");
static_assert(int(Path::Verb::Move) == int(SkPathVerb::kMove) && int(Path::Verb::Line) == int(SkPathVerb::kLine) &&
                  int(Path::Verb::Quad) == int(SkPathVerb::kQuad) &&
                  int(Path::Verb::Conic) == int(SkPathVerb::kConic) &&
                  int(Path::Verb::Cubic) == int(SkPathVerb::kCubic) &&
                  int(Path::Verb::Close) == int(SkPathVerb::kClose),
              "Path::Verb must mirror SkPathVerb");

SkPath path(const Path& p) {
    const auto& pts = p.points();
    const auto& verbs = p.verbs();
    const auto& w = p.conicWeights();
    return SkPath::Raw({reinterpret_cast<const SkPoint*>(pts.data()), pts.size()},
                       {reinterpret_cast<const SkPathVerb*>(verbs.data()), verbs.size()}, {w.data(), w.size()},
                       p.fillRule() == FillRule::EvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
}

Path fromSkPath(const SkPath& sp) {
    Path out;
    out.setFillRule(sp.getFillType() == SkPathFillType::kEvenOdd ? FillRule::EvenOdd : FillRule::NonZero);
    auto pts = sp.points();
    auto weights = sp.conicWeights();
    std::size_t pi = 0, wi = 0;
    for (SkPathVerb v : sp.verbs()) {
        switch (v) {
            case SkPathVerb::kMove: out.moveTo(pts[pi].fX, pts[pi].fY); pi += 1; break;
            case SkPathVerb::kLine: out.lineTo(pts[pi].fX, pts[pi].fY); pi += 1; break;
            case SkPathVerb::kQuad:
                out.quadTo(pts[pi].fX, pts[pi].fY, pts[pi + 1].fX, pts[pi + 1].fY);
                pi += 2;
                break;
            case SkPathVerb::kConic:
                out.conicTo(pts[pi].fX, pts[pi].fY, pts[pi + 1].fX, pts[pi + 1].fY, weights[wi++]);
                pi += 2;
                break;
            case SkPathVerb::kCubic:
                out.cubicTo(pts[pi].fX, pts[pi].fY, pts[pi + 1].fX, pts[pi + 1].fY, pts[pi + 2].fX, pts[pi + 2].fY);
                pi += 3;
                break;
            case SkPathVerb::kClose: out.close(); break;
        }
    }
    return out;
}

SkBlendMode blend(BlendMode m) {
    switch (m) {
        case BlendMode::SrcOver: return SkBlendMode::kSrcOver;
        case BlendMode::Src: return SkBlendMode::kSrc;
        case BlendMode::Clear: return SkBlendMode::kClear;
        case BlendMode::Multiply: return SkBlendMode::kMultiply;
        case BlendMode::Screen: return SkBlendMode::kScreen;
        case BlendMode::Overlay: return SkBlendMode::kOverlay;
        case BlendMode::Darken: return SkBlendMode::kDarken;
        case BlendMode::Lighten: return SkBlendMode::kLighten;
        case BlendMode::ColorDodge: return SkBlendMode::kColorDodge;
        case BlendMode::ColorBurn: return SkBlendMode::kColorBurn;
        case BlendMode::HardLight: return SkBlendMode::kHardLight;
        case BlendMode::SoftLight: return SkBlendMode::kSoftLight;
        case BlendMode::Difference: return SkBlendMode::kDifference;
        case BlendMode::Exclusion: return SkBlendMode::kExclusion;
        case BlendMode::Plus: return SkBlendMode::kPlus;
        case BlendMode::Hue: return SkBlendMode::kHue;
        case BlendMode::Saturation: return SkBlendMode::kSaturation;
        case BlendMode::Color: return SkBlendMode::kColor;
        case BlendMode::Luminosity: return SkBlendMode::kLuminosity;
    }
    return SkBlendMode::kSrcOver;
}

}  // namespace roados::sk
