// SVG engine: parsing, placement, and the documented text normalisation.
#include <gtest/gtest.h>

#include "roados/Graphics.h"

using namespace roados;

namespace {
const char* kTextSvg =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">)"
    R"(<text x="32" y="46" font-family="Inter, 'Segoe UI', Roboto, Arial, sans-serif" font-size="15" )"
    R"(font-weight="800" fill="#111111" text-anchor="middle" dominant-baseline="central">10 t</text></svg>)";
}

TEST(Svg, ParsesSizeAndViewBox) {
    auto doc = SvgDocument::parse(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128" width="128" height="128"><rect width="10" height="10"/></svg>)");
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    EXPECT_EQ(doc->intrinsicSize(), (Size{128, 128}));
    ASSERT_TRUE(doc->viewBox().has_value());
    EXPECT_EQ(*doc->viewBox(), (Rect{0, 0, 128, 128}));
}

TEST(Svg, SizeFallsBackToViewBox) {
    auto doc = SvgDocument::parse(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 32"/>)");
    ASSERT_TRUE(doc.ok());
    EXPECT_EQ(doc->intrinsicSize(), (Size{64, 32}));
}

TEST(Svg, RejectsInvalidMarkup) {
    EXPECT_FALSE(SvgDocument::parse("not svg at all").ok());
    EXPECT_FALSE(SvgDocument::parse("<svg").ok());
    auto missing = SvgDocument::load("/nonexistent/file.svg");
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, ErrorCode::Io);
}

TEST(Svg, NormalisationShiftsYAndResolvesFamily) {
    auto doc = SvgDocument::parse(kTextSvg);
    ASSERT_TRUE(doc.ok());
    const SvgReport& r = doc->report();
    EXPECT_EQ(r.textElements, 1);
    EXPECT_EQ(r.baselinesAdjusted, 1);
    EXPECT_EQ(r.fontFamiliesResolved, 1);
    EXPECT_TRUE(r.warnings.empty());
    const std::string& src = doc->normalizedSource();
    EXPECT_EQ(src.find("dominant-baseline"), std::string::npos);
    EXPECT_EQ(src.find("y=\"46\""), std::string::npos);  // y moved down by (ascent-descent)/2
    // Everything else is untouched.
    EXPECT_NE(src.find("x=\"32\""), std::string::npos);
    EXPECT_NE(src.find("fill=\"#111111\""), std::string::npos);
    EXPECT_NE(src.find(">10 t</text>"), std::string::npos);

    const TextMetrics m = Renderer::shared().measure(Text("x").family("Inter, 'Segoe UI', Roboto, Arial, sans-serif").size(15).weight(800));
    const float expectedY = 46 + (m.ascent - m.descent) / 2;
    const auto pos = src.find(" y=\"");
    ASSERT_NE(pos, std::string::npos);
    EXPECT_NEAR(std::stof(src.substr(pos + 4)), expectedY, 0.001);
}

TEST(Svg, NormalisationCanBeDisabled) {
    auto doc = SvgDocument::parse(kTextSvg, SvgOptions{.normalizeText = false});
    ASSERT_TRUE(doc.ok());
    EXPECT_EQ(doc->normalizedSource(), kTextSvg);
    EXPECT_EQ(doc->report().baselinesAdjusted, 0);
}

TEST(Svg, NormalisationReportsWhatItCannotHandle) {
    auto doc = SvgDocument::parse(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">)"
        R"(<g dominant-baseline="central"><text y="5">a</text></g>)"
        R"(<text y="5" font-family="Arial" font-size="5" dominant-baseline="hanging">b</text></svg>)");
    ASSERT_TRUE(doc.ok());
    EXPECT_EQ(doc->report().baselinesAdjusted, 0);
    ASSERT_EQ(doc->report().warnings.size(), 2u);
    EXPECT_NE(doc->report().warnings[0].find("<g>"), std::string::npos);
    EXPECT_NE(doc->report().warnings[1].find("hanging"), std::string::npos);
}

TEST(Svg, NormalisationLeavesPathsByteIdentical) {
    const std::string svg =
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">)svg"
        R"svg(<path d="M7 41V34c0-2 1.5-3.5 3.5-4l8-1.5 6.5-8.5h15l8 8.5 6 1c2 .5 3 2 3 4V41z" fill="#F2F5F7"/>)svg"
        R"svg(<g transform="translate(36 6) scale(0.34)"><circle cx="1" cy="2" r="3" stroke-dasharray="6 6"/></g></svg>)svg";
    auto doc = SvgDocument::parse(svg);
    ASSERT_TRUE(doc.ok());
    EXPECT_EQ(doc->normalizedSource(), svg);
}

TEST(Svg, ShapePlacementMeetAndStretch) {
    auto doc = SvgDocument::parse(R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="32"/>)");
    ASSERT_TRUE(doc.ok());
    const Transform meet = SvgShape(*doc).position(10, 10).size(128, 128).placement();
    EXPECT_EQ(meet.map({0, 0}), (Point{10, 42}));     // scale 2, centred vertically
    EXPECT_EQ(meet.map({64, 32}), (Point{138, 106}));
    const Transform stretch = SvgShape(*doc).size(128, 128).fit(AspectFit::Stretch).placement();
    EXPECT_EQ(stretch.map({64, 32}), (Point{128, 128}));
}

TEST(Svg, RendersIntoCanvasAndComposesWithShapes) {
    auto doc = SvgDocument::parse(
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">)"
        R"(<rect x="0" y="0" width="32" height="64" fill="#E35D32"/></svg>)");
    ASSERT_TRUE(doc.ok());
    Canvas c(128, 128);
    c.setBackground(Color("#0B1118"));
    c.svg(SvgShape(*doc).size(128, 128));
    c.circle(Circle().center(96, 64).radius(10).fill(Color("#2196F3")));
    auto img = c.render();
    ASSERT_TRUE(img.ok());
    EXPECT_EQ(img->pixel(30, 64), Color("#E35D32"));   // scaled ×2
    EXPECT_EQ(img->pixel(70, 20), Color("#0B1118"));
    EXPECT_EQ(img->pixel(96, 64), Color("#2196F3"));
}

TEST(Svg, EmptyShapeIsAnError) {
    Canvas c(10, 10);
    c.svg(SvgShape());
    auto r = c.render();
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidArgument);
}
