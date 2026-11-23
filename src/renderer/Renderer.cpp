// The single rendering path: walks a roados scene into an SkCanvas. Used for
// raster output (PNG / raw pixels) and for SVG output (SkSVGCanvas).
#include "roados/Renderer.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <tuple>
#include <type_traits>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/svg/SkSVGCanvas.h"
#include "renderer/ImageImpl.h"
#include "renderer/RendererImpl.h"
#include "renderer/SkiaConvert.h"
#include "roados/Canvas.h"
#include "svg/SvgImpl.h"

#if defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(__ANDROID__)
#include "include/ports/SkFontMgr_android.h"
#include "include/ports/SkFontScanner_FreeType.h"
#elif defined(__linux__)
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#endif

namespace roados {

// ---- Fonts -----------------------------------------------------------------

Renderer::Impl::Impl() {
#if defined(__APPLE__)
    fontMgr = SkFontMgr_New_CoreText(nullptr);  // macOS and iOS
#elif defined(__ANDROID__)
    // System fonts from /system/etc/fonts.xml (works on every API level we support).
    fontMgr = SkFontMgr_New_Android(nullptr, SkFontScanner_Make_FreeType());
#elif defined(__linux__)
    fontMgr = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
}

ResolvedTypeface Renderer::Impl::typeface(const Font& f) const {
    const SkFontStyle style(std::clamp(f.weight, 1, 1000), SkFontStyle::kNormal_Width,
                            f.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
    std::string key;
    for (const auto& fam : f.families) key += fam + '\x1f';
    key += std::to_string(f.weight) + (f.italic ? "i" : "n");

    std::lock_guard lock(mutex);
    if (auto it = cache.find(key); it != cache.end()) return it->second;

    ResolvedTypeface out;
    if (fontMgr) {
        for (const auto& fam : f.families) {
            if (fam == "sans-serif" || fam == "serif" || fam == "monospace" || fam == "system-ui") {
                // Generic family: let the platform choose.
                const char* name = fam == "serif" ? "Times New Roman" : fam == "monospace" ? "Courier New" : nullptr;
                out.typeface = name ? fontMgr->matchFamilyStyle(name, style) : nullptr;
                if (!out.typeface) out.typeface = fontMgr->legacyMakeTypeface(nullptr, style);
            } else {
                out.typeface = fontMgr->matchFamilyStyle(fam.c_str(), style);
            }
            if (out.typeface) break;
        }
        if (!out.typeface) out.typeface = fontMgr->legacyMakeTypeface(nullptr, style);
    }
    if (out.typeface) {
        SkString name;
        out.typeface->getFamilyName(&name);
        out.family = name.c_str();
    }
    cache.emplace(key, out);
    return out;
}

// ---- Scene walk --------------------------------------------------------------

namespace {

SkTileMode tile(SpreadMode m) {
    switch (m) {
        case SpreadMode::Pad: return SkTileMode::kClamp;
        case SpreadMode::Repeat: return SkTileMode::kRepeat;
        case SpreadMode::Reflect: return SkTileMode::kMirror;
    }
    return SkTileMode::kClamp;
}

struct Walker {
    SkCanvas* canvas;
    const Renderer::Impl& r;
    std::optional<Error> error;

    void fail(ErrorCode c, std::string m) {
        if (!error) error = makeError(c, std::move(m));
    }

    template <class G>
    bool gradient(SkPaint& paint, const G& g) {
        if (g.stops.empty()) { fail(ErrorCode::InvalidArgument, "gradient has no stops"); return false; }
        std::vector<SkColor4f> colors;
        std::vector<float> pos;
        float prev = 0;
        for (const auto& s : g.stops) {
            prev = std::max(prev, std::clamp(s.offset, 0.0f, 1.0f));  // SVG: offsets never decrease
            colors.push_back(sk::color4f(s.color));
            pos.push_back(prev);
        }
        if (colors.size() == 1) { paint.setColor4f(colors[0], nullptr); return true; }
        SkGradient grad(SkGradient::Colors({colors.data(), colors.size()}, {pos.data(), pos.size()}, tile(g.spread)),
                        SkGradient::Interpolation{});
        sk_sp<SkShader> shader;
        if constexpr (std::is_same_v<G, LinearGradient>) {
            const SkPoint pts[2] = {{g.start.x, g.start.y}, {g.end.x, g.end.y}};
            shader = SkShaders::LinearGradient(pts, grad);
        } else {
            if (!(g.radius > 0)) { fail(ErrorCode::InvalidArgument, "radial gradient radius must be > 0"); return false; }
            shader = SkShaders::RadialGradient({g.center.x, g.center.y}, g.radius, grad);
        }
        if (!shader) { fail(ErrorCode::InvalidArgument, "invalid gradient"); return false; }
        paint.setShader(std::move(shader));
        return true;
    }

    bool brush(SkPaint& paint, const Brush& b) {
        if (const auto* c = std::get_if<Color>(&b)) { paint.setColor4f(sk::color4f(*c), nullptr); return true; }
        if (const auto* l = std::get_if<LinearGradient>(&b)) return gradient(paint, *l);
        return gradient(paint, std::get<RadialGradient>(b));
    }

    bool strokePaint(SkPaint& paint, const Stroke& s) {
        if (!brush(paint, s.brush)) return false;
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(s.width);
        paint.setStrokeMiter(s.miterLimit);
        paint.setStrokeCap(s.cap == LineCap::Round    ? SkPaint::kRound_Cap
                           : s.cap == LineCap::Square ? SkPaint::kSquare_Cap
                                                      : SkPaint::kButt_Cap);
        paint.setStrokeJoin(s.join == LineJoin::Round   ? SkPaint::kRound_Join
                            : s.join == LineJoin::Bevel ? SkPaint::kBevel_Join
                                                        : SkPaint::kMiter_Join);
        if (!s.dash.empty()) {
            std::vector<float> iv = s.dash;
            if (iv.size() % 2) iv.insert(iv.end(), s.dash.begin(), s.dash.end());  // SVG repeats odd lists
            const bool valid = std::all_of(iv.begin(), iv.end(), [](float v) { return v >= 0; }) &&
                               std::any_of(iv.begin(), iv.end(), [](float v) { return v > 0; });
            if (valid) paint.setPathEffect(SkDashPathEffect::Make({iv.data(), iv.size()}, s.dashOffset));
        }
        return true;
    }

    /// Opens an isolated layer when opacity < 1. Returns the blend mode to use for
    /// the shape's own paints (SrcOver when the layer carries the blend instead).
    SkBlendMode beginLayer(const Style& st, bool& layered) {
        SkBlendMode bm = sk::blend(st.blend);
        layered = st.opacity < 1;
        if (layered) {
            SkPaint lp;
            lp.setAlphaf(std::clamp(st.opacity, 0.0f, 1.0f));
            lp.setBlendMode(bm);
            canvas->saveLayer(nullptr, &lp);
            bm = SkBlendMode::kSrcOver;
        }
        return bm;
    }

    void styledPath(const Style& st, const Transform& t, const Path& path, bool open, bool fillDisabled) {
        if (st.opacity <= 0) return;
        std::optional<Brush> fill = st.fill;
        std::optional<Stroke> stroke = st.stroke;
        if (!fill && !stroke) {
            if (open) stroke = Stroke{};                       // lines default to a 1px black stroke
            else if (!fillDisabled) fill = Brush(Color::Black());  // SVG default fill
        }
        const SkPath sp = sk::path(path);
        canvas->save();
        canvas->concat(sk::matrix(t));
        bool layered;
        const SkBlendMode bm = beginLayer(st, layered);
        if (fill && !open) {
            SkPaint p;
            p.setAntiAlias(st.antiAlias);
            p.setBlendMode(bm);
            if (brush(p, *fill)) canvas->drawPath(sp, p);
        }
        if (stroke && stroke->width > 0) {
            SkPaint p;
            p.setAntiAlias(st.antiAlias);
            p.setBlendMode(bm);
            if (strokePaint(p, *stroke)) canvas->drawPath(sp, p);
        }
        if (layered) canvas->restore();
        canvas->restore();
    }

    void text(const Text& t) {
        const Style& st = t.style();
        if (st.opacity <= 0 || t.content().empty()) return;
        const ResolvedTypeface tf = r.typeface(t.font());
        if (!tf.typeface) { fail(ErrorCode::Unsupported, "no font available for text '" + t.content() + "'"); return; }
        SkFont font(tf.typeface, t.font().size);
        font.setEdging(SkFont::Edging::kAntiAlias);
        font.setSubpixel(true);
        const float width = font.measureText(t.content().data(), t.content().size(), SkTextEncoding::kUTF8);
        SkFontMetrics m;
        font.getMetrics(&m);
        const float ascent = -m.fAscent, descent = m.fDescent;
        float x = t.position().x, y = t.position().y;
        if (t.anchor() == TextAnchor::Middle) x -= width / 2;
        else if (t.anchor() == TextAnchor::End) x -= width;
        switch (t.baseline()) {
            case TextBaseline::Alphabetic: break;
            case TextBaseline::Central: y += (ascent - descent) / 2; break;
            case TextBaseline::Top: y += ascent; break;
            case TextBaseline::Bottom: y -= descent; break;
        }
        std::optional<Brush> fill = st.fill;
        if (!fill && !st.stroke && !t.fillDisabled()) fill = Brush(Color::Black());
        canvas->save();
        canvas->concat(sk::matrix(t.transform()));
        bool layered;
        const SkBlendMode bm = beginLayer(st, layered);
        auto draw = [&](SkPaint& p) {
            p.setAntiAlias(st.antiAlias);
            p.setBlendMode(bm);
            canvas->drawSimpleText(t.content().data(), t.content().size(), SkTextEncoding::kUTF8, x, y, font, p);
        };
        if (fill) { SkPaint p; if (brush(p, *fill)) draw(p); }
        if (st.stroke && st.stroke->width > 0) { SkPaint p; if (strokePaint(p, *st.stroke)) draw(p); }
        if (layered) canvas->restore();
        canvas->restore();
    }

    void image(const ImageShape& s) {
        if (s.image().empty()) { fail(ErrorCode::InvalidArgument, "ImageShape has no image"); return; }
        const Style& st = s.style();
        if (st.opacity <= 0) return;
        const auto* impl = s.image().impl();
        sk_sp<SkImage> img = SkImages::RasterFromPixmap(impl->pixmap(), nullptr, nullptr);  // no copy
        if (!img) { fail(ErrorCode::Render, "could not wrap image pixels"); return; }
        SkPaint p;
        p.setAlphaf(std::clamp(st.opacity, 0.0f, 1.0f));
        p.setBlendMode(sk::blend(st.blend));
        p.setAntiAlias(st.antiAlias);
        const SkSamplingOptions sampling(s.smooth() ? SkFilterMode::kLinear : SkFilterMode::kNearest);
        canvas->save();
        canvas->concat(sk::matrix(s.transform()));
        canvas->drawImageRect(img, sk::rect(s.destination()), sampling, &p);
        canvas->restore();
    }

    void svg(const SvgShape& s) {
        if (s.document().empty()) { fail(ErrorCode::InvalidArgument, "SvgShape has no document"); return; }
        const Style& st = s.style();
        if (st.opacity <= 0) return;
        canvas->save();
        canvas->concat(sk::matrix(s.transform()));
        const bool layered = st.opacity < 1 || st.blend != BlendMode::SrcOver;
        if (layered) {
            SkPaint lp;
            lp.setAlphaf(std::clamp(st.opacity, 0.0f, 1.0f));
            lp.setBlendMode(sk::blend(st.blend));
            canvas->saveLayer(nullptr, &lp);
        }
        canvas->concat(sk::matrix(s.placement()));
        s.document().impl()->dom->render(canvas);
        if (layered) canvas->restore();
        canvas->restore();
    }

    void group(const Group& g) {
        if (!g.visible() || g.opacity() <= 0) return;
        canvas->save();
        canvas->concat(sk::matrix(g.transform()));
        if (g.clip()) canvas->clipRect(sk::rect(*g.clip()), true);
        const bool layered = g.opacity() < 1 || g.blend() != BlendMode::SrcOver;
        if (layered) {
            SkPaint lp;
            lp.setAlphaf(std::clamp(g.opacity(), 0.0f, 1.0f));
            lp.setBlendMode(sk::blend(g.blend()));
            canvas->saveLayer(nullptr, &lp);
        }
        for (const auto& child : g.children()) {
            node(child);
            if (error) break;
        }
        if (layered) canvas->restore();
        canvas->restore();
    }

    void node(const Node& n) {
        std::visit(
            [this](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, Group>) group(v);
                else if constexpr (std::is_same_v<T, Text>) text(v);
                else if constexpr (std::is_same_v<T, ImageShape>) image(v);
                else if constexpr (std::is_same_v<T, SvgShape>) svg(v);
                else {
                    constexpr bool open = std::is_same_v<T, Line> || std::is_same_v<T, Polyline>;
                    styledPath(v.style(), v.transform(), v.toPath(), open, v.fillDisabled());
                }
            },
            n.value());
    }
};

Result<void> validateCanvas(const Canvas& c) {
    if (c.width() <= 0 || c.height() <= 0 || c.width() > kMaxRenderDimension || c.height() > kMaxRenderDimension)
        return makeError(ErrorCode::InvalidArgument, "canvas dimensions must be 1.." +
                                                         std::to_string(kMaxRenderDimension));
    return {};
}

}  // namespace

// ---- Renderer ----------------------------------------------------------------

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() = default;

const Renderer& Renderer::shared() {
    static const Renderer instance;  // thread-safe initialisation (C++11 magic statics)
    return instance;
}

Result<void> Renderer::renderInto(const Canvas& c, std::span<std::uint8_t> px, int w, int h, std::size_t stride,
                                  const RenderOptions& o) const {
    if (auto v = validateCanvas(c); !v) return v;
    if (w <= 0 || h <= 0 || w > kMaxRenderDimension || h > kMaxRenderDimension)
        return makeError(ErrorCode::InvalidArgument, "output dimensions out of range");
    if (!(o.scale > 0) || !std::isfinite(o.scale)) return makeError(ErrorCode::InvalidArgument, "scale must be > 0");
    if (stride < std::size_t(w) * 4) return makeError(ErrorCode::InvalidArgument, "stride is smaller than width*4");
    if (px.size() < stride * std::size_t(h - 1) + std::size_t(w) * 4)
        return makeError(ErrorCode::InvalidArgument, "pixel buffer is too small");

    const SkImageInfo info = SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
    sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(info, px.data(), stride);
    if (!surface) return makeError(ErrorCode::Render, "Skia could not create a raster surface");
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(sk::color4f(o.background.value_or(c.background())));
    canvas->scale(o.scale, o.scale);
    Walker walker{canvas, *impl_, std::nullopt};
    walker.group(c.root());
    if (walker.error) return *walker.error;
    return {};
}

Result<Image> Renderer::render(const Canvas& c, const RenderOptions& o) const {
    if (auto v = validateCanvas(c); !v) return v.error();
    if (!(o.scale > 0) || !std::isfinite(o.scale)) return makeError(ErrorCode::InvalidArgument, "scale must be > 0");
    const double wd = std::ceil(double(c.width()) * o.scale), hd = std::ceil(double(c.height()) * o.scale);
    if (wd > kMaxRenderDimension || hd > kMaxRenderDimension)
        return makeError(ErrorCode::InvalidArgument, "scaled output exceeds " + std::to_string(kMaxRenderDimension) + " px");
    auto impl = std::make_shared<Image::Impl>();
    impl->width = int(wd);
    impl->height = int(hd);
    impl->stride = std::size_t(impl->width) * 4;
    impl->pixels.assign(impl->stride * std::size_t(impl->height), 0);
    if (auto r = renderInto(c, impl->pixels, impl->width, impl->height, impl->stride, o); !r) return r.error();
    return Image(std::move(impl));
}

Result<std::string> Renderer::renderSvg(const Canvas& c) const {
    if (auto v = validateCanvas(c); !v) return v.error();
    SkDynamicMemoryWStream stream;
    std::optional<Error> err;
    {
        std::unique_ptr<SkCanvas> svg = SkSVGCanvas::Make(SkRect::MakeWH(float(c.width()), float(c.height())), &stream, 0);
        if (!svg) return makeError(ErrorCode::Render, "Skia could not create an SVG canvas");
        if (c.background().a > 0) {
            SkPaint bg;
            bg.setColor4f(sk::color4f(c.background()), nullptr);
            svg->drawRect(SkRect::MakeWH(float(c.width()), float(c.height())), bg);
        }
        Walker walker{svg.get(), *impl_, std::nullopt};
        walker.group(c.root());
        err = walker.error;
    }  // destroying the SVG canvas writes the closing tags
    if (err) return *err;
    sk_sp<SkData> data = stream.detachAsData();
    return std::string(static_cast<const char*>(data->data()), data->size());
}

TextMetrics Renderer::measure(const Text& t) const {
    TextMetrics out;
    const ResolvedTypeface tf = impl_->typeface(t.font());
    if (!tf.typeface) return out;
    SkFont font(tf.typeface, t.font().size);
    font.setSubpixel(true);
    out.width = font.measureText(t.content().data(), t.content().size(), SkTextEncoding::kUTF8);
    SkFontMetrics m;
    font.getMetrics(&m);
    out.ascent = -m.fAscent;
    out.descent = m.fDescent;
    out.resolvedFamily = tf.family;
    return out;
}

// ---- Canvas convenience ------------------------------------------------------

Result<Image> Canvas::render(const RenderOptions& o) const { return Renderer::shared().render(*this, o); }
Result<std::string> Canvas::toSvg() const { return Renderer::shared().renderSvg(*this); }

}  // namespace roados
