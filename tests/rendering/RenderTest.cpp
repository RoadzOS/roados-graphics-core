// Pixel-level rendering tests through the real Skia pipeline. Probe points are
// chosen away from anti-aliased edges, so exact colour equality is expected
// except where a tolerance is stated.
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <thread>

#include "roados/Graphics.h"

using namespace roados;

namespace {

Image renderOk(const Canvas& c, RenderOptions o = {}) {
    auto r = c.render(o);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? *r : Image();
}

void expectColorNear(Color a, Color b, int tol, const char* where = "") {
    EXPECT_LE(std::abs(int(a.r) - b.r), tol) << where << " got " << a.toHex() << " want " << b.toHex();
    EXPECT_LE(std::abs(int(a.g) - b.g), tol) << where << " got " << a.toHex() << " want " << b.toHex();
    EXPECT_LE(std::abs(int(a.b) - b.b), tol) << where << " got " << a.toHex() << " want " << b.toHex();
    EXPECT_LE(std::abs(int(a.a) - b.a), tol) << where << " got " << a.toHex() << " want " << b.toHex();
}

/// Bounding box of pixels whose alpha exceeds `minAlpha`.
Rect inkBounds(const Image& img, int minAlpha = 0) {
    int l = img.width(), t = img.height(), r = -1, b = -1;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y).a > minAlpha) {
                l = std::min(l, x); t = std::min(t, y);
                r = std::max(r, x); b = std::max(b, y);
            }
    if (r < 0) return {};
    return Rect::fromLTRB(float(l), float(t), float(r + 1), float(b + 1));
}

}  // namespace

TEST(Render, TransparentAndSolidBackgrounds) {
    Canvas c(16, 8);
    Image img = renderOk(c);
    ASSERT_EQ(img.width(), 16);
    ASSERT_EQ(img.height(), 8);
    EXPECT_EQ(img.pixel(3, 3), Color::Transparent());
    c.setBackground(Color("#0B1118"));
    EXPECT_EQ(renderOk(c).pixel(3, 3), Color("#0B1118"));
}

TEST(Render, SolidFillsAreExact) {
    Canvas c(100, 100);
    c.circle(Circle().center(25, 25).radius(20).fill(Color("#2196F3")));
    c.rectangle(Rectangle().position(55, 5).size(40, 40).fill(Color("#D93636")));
    c.triangle(Triangle().points({50, 55}, {95, 95}, {5, 95}).fill(Color("#E2C34C")));
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(25, 25), Color("#2196F3"));
    EXPECT_EQ(img.pixel(75, 25), Color("#D93636"));
    EXPECT_EQ(img.pixel(50, 85), Color("#E2C34C"));
    EXPECT_EQ(img.pixel(2, 2), Color::Transparent());  // outside the circle
}

TEST(Render, DefaultFillIsBlackAndNoFillHonoured) {
    Canvas c(40, 20);
    c.rectangle(Rectangle().position(0, 0).size(20, 20));
    c.rectangle(Rectangle().position(20, 0).size(20, 20).noFill());
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(10, 10), Color::Black());
    EXPECT_EQ(img.pixel(30, 10), Color::Transparent());
}

TEST(Render, StrokeOnlyLeavesInteriorEmpty) {
    Canvas c(100, 100);
    c.roundedRect(RoundedRect().position(10, 10).size(80, 80).radius(10).noFill().stroke(Color("#222222"), 6));
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(50, 10), Color("#222222"));  // on the stroke centre line
    EXPECT_EQ(img.pixel(50, 50), Color::Transparent());
}

TEST(Render, LaterNodesDrawOnTop) {
    Canvas c(20, 20);
    c.rectangle(Rectangle().size(20, 20).fill(Color("#FF0000")));
    c.rectangle(Rectangle().size(20, 20).fill(Color("#0000FF")));
    EXPECT_EQ(renderOk(c).pixel(10, 10), Color("#0000FF"));

    // Reorder: move the blue layer to the bottom.
    ASSERT_TRUE(c.root().move(1, 0));
    EXPECT_EQ(renderOk(c).pixel(10, 10), Color("#FF0000"));
}

TEST(Render, ShapeOpacityComposesFillAndStrokeAsOneLayer) {
    Canvas c(40, 40);
    c.setBackground(Color::White());
    // Opaque fill and stroke at opacity 0.5 → overlap must not double-darken (SVG semantics).
    c.rectangle(Rectangle().position(5, 5).size(30, 30).fill(Color::Black()).stroke(Color::Black(), 6).opacity(0.5f));
    const Image img = renderOk(c);
    expectColorNear(img.pixel(5, 20), Color(127, 127, 127), 1, "stroke over fill");
    expectColorNear(img.pixel(20, 20), Color(127, 127, 127), 1, "fill");
}

TEST(Render, GroupTransformOpacityAndVisibility) {
    Canvas c(100, 100);
    Group g("layer");
    g.transform(Transform::Translate(50, 50)).opacity(0.5f);
    g.add(Rectangle().position(0, 0).size(20, 20).fill(Color("#FF0000")));
    c.group(g);
    Image img = renderOk(c);
    EXPECT_EQ(img.pixel(10, 10), Color::Transparent());  // moved away from the origin
    expectColorNear(img.pixel(60, 60), Color(255, 0, 0, 128), 1);

    c.root().findGroup("layer")->visible(false);
    EXPECT_EQ(renderOk(c).pixel(60, 60), Color::Transparent());
}

TEST(Render, GroupClip) {
    Canvas c(40, 40);
    Group g;
    g.clip({0, 0, 20, 40}).add(Rectangle().size(40, 40).fill(Color("#00FF00")));
    c.group(g);
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(10, 20), Color("#00FF00"));
    EXPECT_EQ(img.pixel(30, 20), Color::Transparent());
}

TEST(Render, ShapeTransformRotation) {
    Canvas c(100, 100);
    // A 40×4 bar along +x from the centre, rotated 90° about the centre → points down.
    c.rectangle(Rectangle().position(50, 48).size(40, 4).fill(Color::Black()).transform(Transform::Rotate(90, 50, 50)));
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(50, 80), Color::Black());
    EXPECT_EQ(img.pixel(80, 50), Color::Transparent());
}

TEST(Render, ScaleOptionProducesHighResolutionOutput) {
    Canvas c(64, 64);
    c.circle(Circle().center(32, 32).radius(16).fill(Color("#E35D32")));
    const Image img = renderOk(c, {.scale = 3});
    ASSERT_EQ(img.width(), 192);
    ASSERT_EQ(img.height(), 192);
    const Rect ink = inkBounds(img);
    EXPECT_NEAR(ink.x, 48, 1);
    EXPECT_NEAR(ink.width, 96, 1);
    EXPECT_EQ(img.pixel(96, 96), Color("#E35D32"));
}

TEST(Render, LinearGradientEndpoints) {
    Canvas c(101, 10);
    c.rectangle(Rectangle().size(101, 10).fill(LinearGradient{{0, 0}, {100, 0}, {{0, Color("#000000")}, {1, Color("#FFFFFF")}}}));
    const Image img = renderOk(c);
    expectColorNear(img.pixel(0, 5), Color("#000000"), 2, "start");
    expectColorNear(img.pixel(50, 5), Color(128, 128, 128), 3, "middle");
    expectColorNear(img.pixel(100, 5), Color("#FFFFFF"), 2, "end");
}

TEST(Render, RadialGradient) {
    Canvas c(100, 100);
    c.rectangle(Rectangle().size(100, 100).fill(RadialGradient{{50, 50}, 50, {{0, Color("#FF0000")}, {1, Color("#0000FF")}}}));
    const Image img = renderOk(c);
    expectColorNear(img.pixel(50, 50), Color("#FF0000"), 6, "centre");
    expectColorNear(img.pixel(0, 0), Color("#0000FF"), 2, "corner (padded)");
}

TEST(Render, DashedStrokeHasGaps) {
    Canvas c(100, 10);
    c.line(Line().from(0, 5).to(100, 5).stroke(Color::Black(), 4).dash({10, 10}));
    const Image img = renderOk(c);
    EXPECT_EQ(img.pixel(5, 5), Color::Black());
    EXPECT_EQ(img.pixel(15, 5), Color::Transparent());
    EXPECT_EQ(img.pixel(25, 5), Color::Black());
}

TEST(Render, LineCapsExtendStroke) {
    Canvas butt(40, 20), square(40, 20);
    butt.line(Line().from(10, 10).to(30, 10).stroke(Color::Black(), 8));
    square.line(Line().from(10, 10).to(30, 10).stroke(Color::Black(), 8).lineCap(LineCap::Square));
    EXPECT_EQ(renderOk(butt).pixel(8, 10), Color::Transparent());
    EXPECT_EQ(renderOk(square).pixel(8, 10), Color::Black());
}

TEST(Render, EvenOddFillRuleCutsHole) {
    auto p = Path::fromSvg("M0 0h40v40h-40z M10 10h20v20h-20z");
    ASSERT_TRUE(p.ok());
    Canvas nonzero(40, 40), evenodd(40, 40);
    nonzero.path(PathShape(*p).fill(Color::Black()));
    p->setFillRule(FillRule::EvenOdd);
    evenodd.path(PathShape(*p).fill(Color::Black()));
    EXPECT_EQ(renderOk(nonzero).pixel(20, 20), Color::Black());
    EXPECT_EQ(renderOk(evenodd).pixel(20, 20), Color::Transparent());
}

TEST(Render, MultiplyBlend) {
    Canvas c(10, 10);
    c.setBackground(Color("#808080"));
    c.rectangle(Rectangle().size(10, 10).fill(Color("#FF8000")).blend(BlendMode::Multiply));
    expectColorNear(renderOk(c).pixel(5, 5), Color(128, 64, 0), 1);
}

TEST(Render, TextCentralBaselineAndMiddleAnchor) {
    Canvas c(200, 100);
    c.text(Text("S4").family("Arial, sans-serif").size(40).weight(800).position(100, 50)
               .anchor(TextAnchor::Middle).baseline(TextBaseline::Central).fill(Color::Black()));
    const Image img = renderOk(c);
    const Rect ink = inkBounds(img, 64);
    ASSERT_FALSE(ink.isEmpty()) << "text produced no pixels";
    // Horizontally centred on x=100; vertical ink centre near y=50 (cap height is
    // smaller than the ascent+descent box, so allow a few px).
    EXPECT_NEAR(ink.center().x, 100, 2);
    EXPECT_NEAR(ink.center().y, 50, 4);
}

TEST(Render, TextMeasurementResolvesFamilyFallback) {
    const TextMetrics m = Renderer::shared().measure(
        Text("120").family("NoSuchFontFamilyXYZ, Arial, sans-serif").size(20));
    EXPECT_GT(m.width, 10);
    EXPECT_GT(m.ascent, 10);
    EXPECT_FALSE(m.resolvedFamily.empty());
#if defined(__APPLE__)
    EXPECT_EQ(m.resolvedFamily, "Arial");  // Arial ships with macOS
#endif
}

TEST(Render, ImageShapeDrawsPixels) {
    std::vector<std::uint8_t> px(2 * 2 * 4);
    for (int i = 0; i < 4; ++i) { px[i * 4 + 0] = 0; px[i * 4 + 1] = 200; px[i * 4 + 2] = 0; px[i * 4 + 3] = 255; }
    auto img = Image::fromPixels(2, 2, px);
    ASSERT_TRUE(img.ok());
    Canvas c(20, 20);
    c.image(ImageShape().image(*img).position(5, 5).size(10, 10).smooth(false));
    const Image out = renderOk(c);
    EXPECT_EQ(out.pixel(10, 10), Color(0, 200, 0));
    EXPECT_EQ(out.pixel(2, 2), Color::Transparent());
}

TEST(Render, RenderIntoCallerBufferWithStride) {
    Canvas c(4, 2);
    c.setBackground(Color("#102030"));
    std::vector<std::uint8_t> buf(32 * 2, 0xAB);  // stride 32 > 4*4
    ASSERT_TRUE(Renderer::shared().renderInto(c, buf, 4, 2, 32).ok());
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[1], 0x20);
    EXPECT_EQ(buf[2], 0x30);
    EXPECT_EQ(buf[3], 0xFF);
    EXPECT_EQ(buf[16], 0xAB);  // padding untouched
    EXPECT_EQ(buf[32], 0x10);  // second row

    auto tooSmall = Renderer::shared().renderInto(c, std::span(buf).first(40), 4, 2, 32);
    ASSERT_FALSE(tooSmall.ok());
    EXPECT_EQ(tooSmall.error().code, ErrorCode::InvalidArgument);
}

TEST(Render, InvalidInputsReturnErrors) {
    EXPECT_EQ(Canvas(0, 10).render().error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(Canvas(10, 10).render({.scale = 0}).error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(Canvas(10000, 10000).render({.scale = 2}).error().code, ErrorCode::InvalidArgument);

    Canvas c(10, 10);
    c.rectangle(Rectangle().size(10, 10).fill(LinearGradient{{0, 0}, {10, 0}, {}}));
    EXPECT_EQ(c.render().error().code, ErrorCode::InvalidArgument);

    Canvas e(10, 10);
    e.image(ImageShape());
    EXPECT_EQ(e.render().error().code, ErrorCode::InvalidArgument);
}

TEST(Render, SvgOutput) {
    Canvas c(64, 64);
    c.setBackground(Color("#0B1118"));
    c.circle(Circle().center(32, 32).radius(10).fill(Color("#D93636")));
    auto svg = c.toSvg();
    ASSERT_TRUE(svg.ok());
    EXPECT_NE(svg->find("<svg"), std::string::npos);
    EXPECT_NE(svg->find("width=\"64\""), std::string::npos);
    EXPECT_NE(svg->find("#D93636"), std::string::npos);
    EXPECT_NE(svg->find("#0B1118"), std::string::npos);
    EXPECT_NE(svg->find("</svg>"), std::string::npos);
}

TEST(Render, ConcurrentRenderingIsDeterministic) {
    Canvas c(128, 128);
    c.circle(Circle().center(64, 64).radius(40).fill(Color("#E99A32")).stroke(Color("#0B1118"), 4));
    c.text(Text("200 m").size(14).position(64, 118).anchor(TextAnchor::Middle).baseline(TextBaseline::Central));
    const Image ref = renderOk(c);
    std::vector<std::thread> threads;
    std::atomic<int> mismatches{0};
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&] {
            for (int i = 0; i < 10; ++i) {
                auto img = c.render();
                if (!img || !std::equal(img->pixels().begin(), img->pixels().end(), ref.pixels().begin()))
                    ++mismatches;
            }
        });
    for (auto& th : threads) th.join();
    EXPECT_EQ(mismatches.load(), 0);
}
