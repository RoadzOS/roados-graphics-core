#include "roados/binding/Binding.h"

#include <cmath>
#include <map>
#include <mutex>

#include "roados/Version.h"

namespace roados::binding {

std::string versionString() {
    const auto v = libraryVersion();
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
}

namespace {

Error invalid(std::string msg) { return makeError(ErrorCode::InvalidArgument, std::move(msg)); }

Color fromArgb(std::uint32_t argb) {
    return {static_cast<std::uint8_t>(argb >> 16), static_cast<std::uint8_t>(argb >> 8), static_cast<std::uint8_t>(argb),
            static_cast<std::uint8_t>(argb >> 24)};
}

bool finite(float v) { return std::isfinite(v); }

Result<std::optional<Brush>> toBrush(const PaintSpec& p, const char* what) {
    const std::string w(what);
    if (p.spread < 0 || p.spread > 2) return invalid(w + ": spread must be 0 (pad), 1 (repeat) or 2 (reflect)");
    auto stops = [&]() -> Result<std::vector<GradientStop>> {
        if (p.stopColors.size() != p.stopOffsets.size())
            return invalid(w + ": gradient has " + std::to_string(p.stopColors.size()) + " colours but " +
                           std::to_string(p.stopOffsets.size()) + " offsets");
        if (p.stopColors.empty()) return invalid(w + ": a gradient needs at least one stop");
        std::vector<GradientStop> out;
        float prev = 0;
        for (std::size_t i = 0; i < p.stopColors.size(); ++i) {
            const float o = p.stopOffsets[i];
            if (!finite(o) || o < 0 || o > 1) return invalid(w + ": gradient stop offsets must be within 0…1");
            if (o < prev) return invalid(w + ": gradient stop offsets must not decrease");
            prev = o;
            out.push_back({o, fromArgb(p.stopColors[i])});
        }
        return out;
    };
    const auto spread = static_cast<SpreadMode>(p.spread);
    switch (p.kind) {
        case 0: return std::optional<Brush>{};
        case 1: return std::optional<Brush>{Brush{fromArgb(p.argb)}};
        case 2: {
            auto s = stops();
            if (!s) return s.error();
            if (!finite(p.x0) || !finite(p.y0) || !finite(p.x1) || !finite(p.y1))
                return invalid(w + ": gradient coordinates must be finite");
            return std::optional<Brush>{Brush{LinearGradient{{p.x0, p.y0}, {p.x1, p.y1}, std::move(*s), spread}}};
        }
        case 3: {
            auto s = stops();
            if (!s) return s.error();
            if (!finite(p.radius) || p.radius <= 0) return invalid(w + ": radial gradient radius must be > 0");
            return std::optional<Brush>{Brush{RadialGradient{{p.x0, p.y0}, p.radius, std::move(*s), spread}}};
        }
        default: return invalid(w + ": paint kind must be 0 (none), 1 (solid), 2 (linear) or 3 (radial)");
    }
}

/// Style plus how it should be applied to a Styled<T> shape.
struct Applied {
    Style style;
    Transform transform;
    bool explicitNoFill = false;
};

Result<Applied> resolve(const StyleSpec& spec) {
    auto st = toStyle(spec);
    if (!st) return st.error();
    // SVG defaults: with neither fill nor stroke, closed shapes are filled black.
    // With only a stroke, the fill is explicitly off.
    return Applied{*st, toTransform(spec.transform), spec.fill.kind == 0 && spec.stroke.kind != 0};
}

template <class T>
T& apply(T& shape, const Applied& a) {
    if (a.style.fill) shape.fill(*a.style.fill);
    else if (a.explicitNoFill) shape.noFill();
    if (a.style.stroke) shape.stroke(*a.style.stroke);
    shape.opacity(a.style.opacity).blend(a.style.blend).antiAlias(a.style.antiAlias).transform(a.transform);
    return shape;
}

}  // namespace

Transform toTransform(const float m[6]) { return Transform::Matrix(m[0], m[1], m[2], m[3], m[4], m[5]); }

Result<Style> toStyle(const StyleSpec& spec) {
    Style s;
    auto fill = toBrush(spec.fill, "fill");
    if (!fill) return fill.error();
    s.fill = std::move(*fill);
    auto strokeBrush = toBrush(spec.stroke, "stroke");
    if (!strokeBrush) return strokeBrush.error();
    if (*strokeBrush) {
        if (!finite(spec.strokeWidth) || spec.strokeWidth < 0) return invalid("stroke width must be ≥ 0");
        if (spec.lineCap < 0 || spec.lineCap > 2) return invalid("lineCap must be 0 (butt), 1 (round) or 2 (square)");
        if (spec.lineJoin < 0 || spec.lineJoin > 2) return invalid("lineJoin must be 0 (miter), 1 (round) or 2 (bevel)");
        for (float d : spec.dash)
            if (!finite(d) || d < 0) return invalid("dash lengths must be ≥ 0");
        Stroke st;
        st.brush = std::move(**strokeBrush);
        st.width = spec.strokeWidth;
        st.cap = static_cast<LineCap>(spec.lineCap);
        st.join = static_cast<LineJoin>(spec.lineJoin);
        st.miterLimit = spec.miterLimit;
        st.dash = spec.dash;
        st.dashOffset = spec.dashOffset;
        s.stroke = std::move(st);
    }
    if (!finite(spec.opacity) || spec.opacity < 0 || spec.opacity > 1) return invalid("opacity must be within 0…1");
    s.opacity = spec.opacity;
    if (spec.blend < 0 || spec.blend > static_cast<int>(BlendMode::Luminosity)) return invalid("unknown blend mode");
    s.blend = static_cast<BlendMode>(spec.blend);
    for (float v : spec.transform)
        if (!finite(v)) return invalid("transform must be finite");
    return s;
}

Result<StyleSpec> decodeStyle(std::span<const std::int32_t> ints, std::span<const float> floats) {
    constexpr std::size_t kInts = 13, kFloats = 20;
    if (ints.size() < kInts || floats.size() < kFloats) return invalid("style encoding is truncated");
    if (ints[0] != 1) return makeError(ErrorCode::Incompatible, "unsupported style encoding version " + std::to_string(ints[0]));
    const auto nFill = static_cast<std::size_t>(std::max(0, ints[10]));
    const auto nStroke = static_cast<std::size_t>(std::max(0, ints[11]));
    const auto nDash = static_cast<std::size_t>(std::max(0, ints[12]));
    if (ints.size() != kInts + nFill + nStroke || floats.size() != kFloats + nFill + nStroke + nDash)
        return invalid("style encoding has inconsistent lengths");

    StyleSpec s;
    auto u = [&](std::size_t i) { return static_cast<std::uint32_t>(ints[i]); };
    s.fill.kind = ints[1]; s.fill.argb = u(2); s.fill.spread = ints[3];
    s.stroke.kind = ints[4]; s.stroke.argb = u(5); s.stroke.spread = ints[6];
    s.lineCap = ints[7]; s.lineJoin = ints[8]; s.blend = ints[9];
    s.strokeWidth = floats[0]; s.miterLimit = floats[1]; s.dashOffset = floats[2]; s.opacity = floats[3];
    for (int i = 0; i < 6; ++i) s.transform[i] = floats[4 + static_cast<std::size_t>(i)];
    s.fill.x0 = floats[10]; s.fill.y0 = floats[11]; s.fill.x1 = floats[12]; s.fill.y1 = floats[13]; s.fill.radius = floats[14];
    s.stroke.x0 = floats[15]; s.stroke.y0 = floats[16]; s.stroke.x1 = floats[17]; s.stroke.y1 = floats[18]; s.stroke.radius = floats[19];
    std::size_t ii = kInts, fi = kFloats;
    for (std::size_t k = 0; k < nFill; ++k) s.fill.stopColors.push_back(u(ii++));
    for (std::size_t k = 0; k < nStroke; ++k) s.stroke.stopColors.push_back(u(ii++));
    for (std::size_t k = 0; k < nFill; ++k) s.fill.stopOffsets.push_back(floats[fi++]);
    for (std::size_t k = 0; k < nStroke; ++k) s.stroke.stopOffsets.push_back(floats[fi++]);
    for (std::size_t k = 0; k < nDash; ++k) s.dash.push_back(floats[fi++]);
    return s;
}

// ------------------------------------------------------------------ hazard

Result<rvhl::HazardOptions> toHazardOptions(const HazardSpec& s) {
    using namespace rvhl;
    HazardOptions o;
    if (!s.variant.empty()) o.variant = s.variant;
    if (s.severity < 0 || s.severity > 5) return invalid("severity must be 0 (default) or 1…5");
    if (s.severity > 0) o.severity = static_cast<Severity>(s.severity);
    if (s.theme < 0 || s.theme > 1) return invalid("theme must be 0 (dark) or 1 (light)");
    o.theme = s.theme == 0 ? Theme::Dark : Theme::Light;
    if (s.representation < 0 || s.representation > 2) return invalid("representation must be 0 (auto), 1 (standard) or 2 (roados)");
    o.representation = static_cast<Representation>(s.representation);
    if (s.sizeVariant < -1 || s.sizeVariant > 3) return invalid("sizeVariant must be -1 (none) or 0…3 (S, M, L, XL)");
    if (s.sizeVariant >= 0) o.sizeVariant = static_cast<SizeVariant>(s.sizeVariant);
    if (s.count < 0) return invalid("count must be ≥ 0");
    if (s.count > 0) o.count = s.count;
    if (s.speedKind < -1 || s.speedKind > 2) return invalid("speedKind must be -1 (none), 0 (posted), 1 (advisory) or 2 (unknown)");
    if (s.speedUnit < 0 || s.speedUnit > 1) return invalid("speedUnit must be 0 (km/h) or 1 (mph)");
    if (s.speedKind >= 0) {
        SpeedBadge b;
        b.kind = static_cast<SpeedBadge::Kind>(s.speedKind);
        if (s.speedKmh >= 0) b.valueKmh = s.speedKmh;
        b.unit = s.speedUnit == 0 ? SpeedUnit::Kmh : SpeedUnit::Mph;
        o.speed = b;
    }
    if (s.distanceM >= 0) o.distanceM = s.distanceM;
    if (s.unitSystem < 0 || s.unitSystem > 1) return invalid("unitSystem must be 0 (metric) or 1 (imperial)");
    o.unitSystem = s.unitSystem == 0 ? UnitSystem::Metric : UnitSystem::Imperial;
    if (s.direction < -1 || s.direction > 3) return invalid("direction must be -1 (none) or 0…3 (left, ahead, right, both)");
    if (s.direction >= 0) {
        static constexpr Direction dirs[] = {Direction::Left, Direction::Ahead, Direction::Right, Direction::Both};
        o.direction = dirs[s.direction];
    }
    if (s.laneCount < 0) return invalid("laneCount must be ≥ 0");
    if (s.laneCount > 0) o.lane = Lane{s.laneCount, s.laneIndex};
    o.showSeverityCode = s.showSeverityCode;
    if (s.hasBackground) o.background = fromArgb(s.backgroundArgb);
    if (!s.layers.empty()) {
        o.layers.clear();
        for (int l : s.layers) {
            if (l < 0 || l > static_cast<int>(Layer::Distance)) return invalid("unknown layer " + std::to_string(l));
            o.layers.push_back(static_cast<Layer>(l));
        }
    }
    return o;
}

Result<rvhl::Hazard> makeHazard(const rvhl::Library& library, const HazardSpec& spec) {
    auto h = rvhl::Hazard::create(library, spec.id);
    if (!h) return h.error();
    auto opts = toHazardOptions(spec);
    if (!opts) return opts.error();
    h->options() = std::move(*opts);
    h->size(spec.width, spec.height);
    return h;
}

Result<rvhl::Library> sharedLibrary(const std::filesystem::path& bundleDir) {
    static std::mutex mutex;
    static std::map<std::string, rvhl::Library> cache;
    // Same directory → same key, however it is spelled ("dir", "dir/", "dir/.", symlinks).
    std::error_code ec;
    std::filesystem::path norm = std::filesystem::weakly_canonical(bundleDir, ec);
    if (ec) norm = bundleDir.lexically_normal();
    if (!norm.has_filename() && norm.has_parent_path()) norm = norm.parent_path();
    const std::string key = norm.string();
    std::lock_guard lock(mutex);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    auto lib = rvhl::Library::load(bundleDir);
    if (!lib) return lib.error();
    cache.emplace(key, *lib);
    return lib;
}

// ------------------------------------------------------------------ scene

SceneBuilder::SceneBuilder(int width, int height) : canvas_(width, height) {}

void SceneBuilder::setBackground(std::uint32_t argb) { canvas_.setBackground(fromArgb(argb)); }

void SceneBuilder::add(Node n) {
    if (stack_.empty()) canvas_.draw(std::move(n));
    else stack_.back().add(std::move(n));
}

#define ROADOS_RESOLVE(s)          \
    auto applied = resolve(s);     \
    if (!applied) return applied.error()

Result<void> SceneBuilder::circle(float cx, float cy, float r, const StyleSpec& s) {
    if (!finite(r) || r < 0) return invalid("circle radius must be ≥ 0");
    ROADOS_RESOLVE(s);
    Circle c;
    c.center(cx, cy).radius(r);
    add(apply(c, *applied));
    return {};
}

Result<void> SceneBuilder::ellipse(float cx, float cy, float rx, float ry, const StyleSpec& s) {
    if (!finite(rx) || !finite(ry) || rx < 0 || ry < 0) return invalid("ellipse radii must be ≥ 0");
    ROADOS_RESOLVE(s);
    Ellipse e;
    e.center(cx, cy).radii(rx, ry);
    add(apply(e, *applied));
    return {};
}

Result<void> SceneBuilder::rect(float x, float y, float w, float h, float rx, float ry, const StyleSpec& s) {
    if (!finite(w) || !finite(h) || w < 0 || h < 0) return invalid("rectangle size must be ≥ 0");
    if (rx < 0 || ry < 0) return invalid("corner radii must be ≥ 0");
    ROADOS_RESOLVE(s);
    if (rx > 0 || ry > 0) {
        RoundedRect r;
        r.position(x, y).size(w, h).radii(rx, ry > 0 ? ry : rx);
        add(apply(r, *applied));
    } else {
        Rectangle r;
        r.position(x, y).size(w, h);
        add(apply(r, *applied));
    }
    return {};
}

Result<void> SceneBuilder::line(float x1, float y1, float x2, float y2, const StyleSpec& s) {
    ROADOS_RESOLVE(s);
    Line l;
    l.from(x1, y1).to(x2, y2);
    add(apply(l, *applied));
    return {};
}

Result<void> SceneBuilder::poly(std::span<const float> xy, bool closed, const StyleSpec& s) {
    if (xy.size() % 2 != 0) return invalid("point list must contain x,y pairs");
    if (xy.size() < 4) return invalid("a polyline or polygon needs at least two points");
    ROADOS_RESOLVE(s);
    std::vector<Point> pts;
    for (std::size_t i = 0; i < xy.size(); i += 2) pts.push_back({xy[i], xy[i + 1]});
    if (closed) {
        Polygon p;
        p.points(std::move(pts));
        add(apply(p, *applied));
    } else {
        Polyline p;
        p.points(std::move(pts));
        add(apply(p, *applied));
    }
    return {};
}

Result<void> SceneBuilder::path(std::string_view d, bool evenOdd, const StyleSpec& s) {
    auto p = Path::fromSvg(d);
    if (!p) return p.error();
    p->setFillRule(evenOdd ? FillRule::EvenOdd : FillRule::NonZero);
    ROADOS_RESOLVE(s);
    PathShape ps(std::move(*p));
    add(apply(ps, *applied));
    return {};
}

Result<void> SceneBuilder::text(std::string_view utf8, float x, float y, std::string_view cssFamilies, float size,
                                int weight, bool italic, int anchor, int baseline, const StyleSpec& s) {
    if (!finite(size) || size <= 0) return invalid("font size must be > 0");
    if (weight < 1 || weight > 1000) return invalid("font weight must be 1…1000");
    if (anchor < 0 || anchor > 2) return invalid("text anchor must be 0 (start), 1 (middle) or 2 (end)");
    if (baseline < 0 || baseline > 3) return invalid("text baseline must be 0…3");
    ROADOS_RESOLVE(s);
    Text t{std::string(utf8)};
    t.position(x, y).size(size).weight(weight).italic(italic)
        .anchor(static_cast<TextAnchor>(anchor)).baseline(static_cast<TextBaseline>(baseline));
    if (!cssFamilies.empty()) t.family(cssFamilies);
    add(apply(t, *applied));
    return {};
}

Result<void> SceneBuilder::svg(std::string_view markup, float x, float y, float w, float h, const StyleSpec& s) {
    auto doc = SvgDocument::parse(markup);
    if (!doc) return doc.error();
    ROADOS_RESOLVE(s);
    SvgShape shape(std::move(*doc));
    shape.position(x, y).size(w, h);
    shape.opacity(applied->style.opacity).blend(applied->style.blend).transform(applied->transform);
    add(std::move(shape));
    return {};
}

Result<void> SceneBuilder::image(std::span<const std::uint8_t> rgba, int w, int h, std::size_t stride, float x, float y,
                                 float dw, float dh, const StyleSpec& s) {
    auto img = Image::fromPixels(w, h, rgba, stride);
    if (!img) return img.error();
    ROADOS_RESOLVE(s);
    ImageShape shape;
    shape.image(std::move(*img)).position(x, y).size(dw, dh);
    shape.opacity(applied->style.opacity).blend(applied->style.blend).transform(applied->transform);
    add(std::move(shape));
    return {};
}

Result<void> SceneBuilder::hazard(const rvhl::Library& library, const HazardSpec& spec, float x, float y, float w,
                                  float h) {
    auto hz = makeHazard(library, spec);
    if (!hz) return hz.error();
    auto composed = hz->compose();
    if (!composed) return composed.error();
    auto doc = SvgDocument::parse(composed->svg);
    if (!doc) return doc.error();
    SvgShape shape(std::move(*doc));
    shape.position(x, y).size(w, h);
    add(std::move(shape));
    return {};
}

Result<void> SceneBuilder::beginGroup(const float transform[6], float opacity, bool clip, float cx, float cy, float cw,
                                      float ch) {
    if (!finite(opacity) || opacity < 0 || opacity > 1) return invalid("group opacity must be within 0…1");
    for (int i = 0; i < 6; ++i)
        if (!finite(transform[i])) return invalid("group transform must be finite");
    Group g;
    g.transform(toTransform(transform)).opacity(opacity);
    if (clip) g.clip(Rect{cx, cy, cw, ch});
    stack_.push_back(std::move(g));
    return {};
}

Result<void> SceneBuilder::endGroup() {
    if (stack_.empty()) return invalid("endGroup() without a matching beginGroup()");
    Group g = std::move(stack_.back());
    stack_.pop_back();
    add(std::move(g));
    return {};
}

void SceneBuilder::clear() {
    canvas_.clear();
    stack_.clear();
}

Canvas SceneBuilder::finished() const {
    if (stack_.empty()) return canvas_;
    Canvas c = canvas_;
    Group open = stack_.back();
    for (auto it = stack_.rbegin() + 1; it != stack_.rend(); ++it) {
        Group parent = *it;
        parent.add(std::move(open));
        open = std::move(parent);
    }
    c.group(std::move(open));
    return c;
}

Result<Image> SceneBuilder::render(float scale) const {
    RenderOptions o;
    o.scale = scale;
    return finished().render(o);
}

Result<void> SceneBuilder::renderInto(std::span<std::uint8_t> rgba, int widthPx, int heightPx, std::size_t stride,
                                      float scale) const {
    RenderOptions o;
    o.scale = scale;
    return Renderer::shared().renderInto(finished(), rgba, widthPx, heightPx, stride, o);
}

Result<std::string> SceneBuilder::toSvg() const { return finished().toSvg(); }

}  // namespace roados::binding
