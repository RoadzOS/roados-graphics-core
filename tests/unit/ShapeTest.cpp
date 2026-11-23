#include <gtest/gtest.h>

#include "roados/Shape.h"

using namespace roados;

namespace {
void expectBounds(const Path& p, float x, float y, float w, float h, float eps = 1e-3f) {
    const Rect r = p.bounds();
    EXPECT_NEAR(r.x, x, eps);
    EXPECT_NEAR(r.y, y, eps);
    EXPECT_NEAR(r.width, w, eps);
    EXPECT_NEAR(r.height, h, eps);
}
}  // namespace

TEST(Shapes, GeometryOfBasicShapes) {
    expectBounds(Circle().center(120, 120).radius(50).toPath(), 70, 70, 100, 100);
    expectBounds(Ellipse().center(50, 50).radii(30, 10).toPath(), 20, 40, 60, 20);
    expectBounds(Rectangle().position(5, 6).size(7, 8).toPath(), 5, 6, 7, 8);
    expectBounds(RoundedRect().position(200, 100).size(180, 120).radius(20).toPath(), 200, 100, 180, 120);
    expectBounds(Square().position(10, 10).size(30).toPath(), 10, 10, 30, 30);
    expectBounds(Triangle().points({64, 10}, {111, 94}, {17, 94}).toPath(), 17, 10, 94, 84);
    expectBounds(Line().from(30, 300).to(450, 300).toPath(), 30, 300, 420, 0);
}

TEST(Shapes, RegularPolygon) {
    const Polygon hex = Polygon::regular(6, 0, 0, 10);
    ASSERT_EQ(hex.points().size(), 6u);
    EXPECT_NEAR(hex.points()[0].x, 0, 1e-5);   // first vertex straight up
    EXPECT_NEAR(hex.points()[0].y, -10, 1e-5);
    EXPECT_TRUE(Polygon::regular(2, 0, 0, 10).points().empty());
}

TEST(Shapes, PolygonIsClosedPolylineIsOpen) {
    const std::vector<Point> pts{{0, 0}, {10, 0}, {10, 10}};
    EXPECT_EQ(Polygon().points(pts).toPath().verbs().back(), Path::Verb::Close);
    EXPECT_EQ(Polyline().points(pts).toPath().verbs().back(), Path::Verb::Line);
}

TEST(Shapes, BuilderStylingDefaults) {
    const Circle c;
    EXPECT_FALSE(c.style().fill.has_value());
    EXPECT_FALSE(c.style().stroke.has_value());
    EXPECT_FLOAT_EQ(c.style().opacity, 1.f);
    EXPECT_EQ(c.style().blend, BlendMode::SrcOver);
    EXPECT_TRUE(c.transform().isIdentity());
}

TEST(Shapes, BuilderSetsStroke) {
    auto r = RoundedRect().stroke(Color("#222222"), 3).lineJoin(LineJoin::Round).dash({4, 3}, 1);
    ASSERT_TRUE(r.style().stroke.has_value());
    const Stroke& s = *r.style().stroke;
    EXPECT_EQ(std::get<Color>(s.brush), Color("#222222"));
    EXPECT_FLOAT_EQ(s.width, 3);
    EXPECT_EQ(s.join, LineJoin::Round);
    EXPECT_EQ(s.dash, (std::vector<float>{4, 3}));
    EXPECT_FLOAT_EQ(s.dashOffset, 1);
}

TEST(Shapes, RoundedRectRadiusFallback) {
    // rx only: ry takes rx (SVG rule) → bounds unchanged, path is not a plain rect.
    const Path p = RoundedRect().position(0, 0).size(20, 10).radii(4, 0).toPath();
    EXPECT_GT(p.verbs().size(), 5u);
}

TEST(Shapes, ImageShapeDefaultsToImageSize) {
    std::vector<std::uint8_t> px(4 * 3 * 2, 255);
    auto img = Image::fromPixels(3, 2, px);
    ASSERT_TRUE(img.ok());
    const ImageShape s = ImageShape().image(*img).position(5, 6);
    EXPECT_EQ(s.destination(), (Rect{5, 6, 3, 2}));
}
