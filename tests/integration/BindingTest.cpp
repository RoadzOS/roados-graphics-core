// The binding facade that the Android (JNI) and iOS (Objective-C++) bridges call.
#include <gtest/gtest.h>

#include <filesystem>

#include "roados/binding/Binding.h"

using namespace roados;
using namespace roados::binding;
namespace fs = std::filesystem;

namespace {
StyleSpec solidFill(std::uint32_t argb) {
    StyleSpec s;
    s.fill.kind = 1;
    s.fill.argb = argb;
    return s;
}
const fs::path kBundle = fs::path(ROADOS_ASSETS_DIR) / "rvhl";
}  // namespace

TEST(Binding, StyleDefaultsFollowSvg) {
    // No fill and no stroke: closed shapes are filled black (SVG default).
    SceneBuilder b(10, 10);
    ASSERT_TRUE(b.rect(0, 0, 10, 10, 0, 0, StyleSpec{}));
    auto img = b.render();
    ASSERT_TRUE(img);
    EXPECT_EQ(img->pixel(5, 5), Color::Black());

    // Stroke only: interior stays transparent.
    SceneBuilder c(20, 20);
    StyleSpec stroke;
    stroke.stroke.kind = 1;
    stroke.stroke.argb = 0xFFFF0000;
    stroke.strokeWidth = 2;
    ASSERT_TRUE(c.rect(2, 2, 16, 16, 0, 0, stroke));
    auto img2 = c.render();
    ASSERT_TRUE(img2);
    EXPECT_EQ(img2->pixel(10, 10).a, 0);
    EXPECT_EQ(img2->pixel(2, 10), Color("#FF0000"));
}

TEST(Binding, RejectsInvalidInputInsteadOfClamping) {
    SceneBuilder b(10, 10);
    StyleSpec s = solidFill(0xFF000000);
    s.opacity = 1.5f;
    EXPECT_EQ(b.circle(5, 5, 2, s).error().code, ErrorCode::InvalidArgument);
    EXPECT_FALSE(b.circle(5, 5, -1, solidFill(0xFF000000)));
    StyleSpec g;
    g.fill.kind = 2;
    g.fill.stopColors = {0xFF000000, 0xFFFFFFFF};
    g.fill.stopOffsets = {0.0f};
    EXPECT_FALSE(b.rect(0, 0, 5, 5, 0, 0, g));
    EXPECT_FALSE(b.path("M0 0 Q", false, solidFill(0xFF000000)));
    EXPECT_FALSE(b.endGroup());
}

TEST(Binding, GroupsNestAndCloseForRendering) {
    SceneBuilder b(40, 20);
    const float shift[6] = {1, 0, 0, 1, 20, 0};
    ASSERT_TRUE(b.beginGroup(shift, 1, false, 0, 0, 0, 0));
    ASSERT_TRUE(b.rect(0, 0, 10, 10, 0, 0, solidFill(0xFF00FF00)));
    EXPECT_EQ(b.groupDepth(), 1);
    // Rendering with the group still open includes it.
    auto open = b.render();
    ASSERT_TRUE(open);
    EXPECT_EQ(open->pixel(25, 5), Color("#00FF00"));
    ASSERT_TRUE(b.endGroup());
    auto img = b.render();
    ASSERT_TRUE(img);
    EXPECT_EQ(img->pixel(25, 5), Color("#00FF00"));
    EXPECT_EQ(img->pixel(5, 5).a, 0);
}

TEST(Binding, DecodesCompactStyleEncoding) {
    // Linear-gradient fill with 2 stops, red stroke, 1 dash value.
    const std::vector<std::int32_t> ints = {
        1, 2, 0, 0, 1, static_cast<std::int32_t>(0xFFFF0000u), 0, 1, 1, 0, 2, 0, 1,
        static_cast<std::int32_t>(0xFF000000u), static_cast<std::int32_t>(0xFFFFFFFFu)};
    const std::vector<float> floats = {3, 4, 0, 0.5f, 1, 0, 0, 1, 10, 20,   // width, miter, dashOff, opacity, matrix
                                       0, 0, 100, 0, 0,  0, 0, 0, 0, 0,     // fill geometry, stroke geometry
                                       0, 1,                                // fill stop offsets
                                       6};                                  // dash
    auto s = decodeStyle(ints, floats);
    ASSERT_TRUE(s) << s.error().message;
    EXPECT_EQ(s->fill.kind, 2);
    EXPECT_EQ(s->fill.x1, 100);
    EXPECT_EQ(s->fill.stopColors.size(), 2u);
    EXPECT_EQ(s->stroke.argb, 0xFFFF0000u);
    EXPECT_EQ(s->strokeWidth, 3);
    EXPECT_EQ(s->opacity, 0.5f);
    EXPECT_EQ(s->transform[4], 10);
    EXPECT_EQ(s->dash, std::vector<float>{6});
    EXPECT_TRUE(toStyle(*s));

    auto truncated = decodeStyle(std::span(ints).first(5), floats);
    EXPECT_FALSE(truncated);
    auto badVersion = ints;
    badVersion[0] = 2;
    EXPECT_EQ(decodeStyle(badVersion, floats).error().code, ErrorCode::Incompatible);
}

TEST(Binding, HazardSpecMapsToOptions) {
    HazardSpec spec;
    spec.id = "RDM-003";
    spec.severity = 4;
    spec.theme = 1;
    spec.speedKind = 1;
    spec.speedKmh = 30;
    spec.distanceM = 150;
    spec.layers = {static_cast<int>(rvhl::Layer::Frame), static_cast<int>(rvhl::Layer::Pictogram)};
    auto o = toHazardOptions(spec);
    ASSERT_TRUE(o) << o.error().message;
    EXPECT_EQ(o->severity, rvhl::Severity::S4);
    EXPECT_EQ(o->theme, rvhl::Theme::Light);
    ASSERT_TRUE(o->speed);
    EXPECT_EQ(o->speed->kind, rvhl::SpeedBadge::Kind::Advisory);
    EXPECT_EQ(o->layers.size(), 2u);

    spec.severity = 9;
    EXPECT_FALSE(toHazardOptions(spec));
}

TEST(Binding, SharedLibraryIsCachedAndHazardsRender) {
    auto a = sharedLibrary(kBundle);
    ASSERT_TRUE(a) << a.error().message;
    auto b = sharedLibrary(kBundle / ".");
    ASSERT_TRUE(b);
    EXPECT_EQ(a->impl(), b->impl());

    HazardSpec spec;
    spec.id = "large_pothole";
    spec.width = spec.height = 96;
    auto h = makeHazard(*a, spec);
    ASSERT_TRUE(h) << h.error().message;
    auto img = h->render();
    ASSERT_TRUE(img);
    EXPECT_EQ(img->width(), 96);

    SceneBuilder scene(200, 100);
    ASSERT_TRUE(scene.hazard(*a, spec, 0, 0, 100, 100));
    ASSERT_TRUE(scene.circle(150, 50, 20, solidFill(0xFF2196F3)));
    auto out = scene.render();
    ASSERT_TRUE(out);
    EXPECT_EQ(out->pixel(150, 50), Color("#2196F3"));
    EXPECT_GT(out->pixel(50, 50).a, 0);

    spec.id = "pothole";
    EXPECT_EQ(makeHazard(*a, spec).error().code, ErrorCode::NotFound);
}

TEST(Binding, RenderIntoCallerMemory) {
    SceneBuilder b(4, 4);
    ASSERT_TRUE(b.rect(0, 0, 4, 4, 0, 0, solidFill(0xFF0000FF)));
    std::vector<std::uint8_t> px(4 * 4 * 4, 0);
    ASSERT_TRUE(b.renderInto(px, 4, 4, 16));
    EXPECT_EQ(px[0], 0);    // R
    EXPECT_EQ(px[2], 255);  // B
    EXPECT_EQ(px[3], 255);  // A
}
