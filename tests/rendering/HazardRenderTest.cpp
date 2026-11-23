// Rendering composed RVHL icons through Skia.
#include <gtest/gtest.h>

#include <filesystem>

#include "roados/rvhl/Rvhl.h"

using namespace roados;
using namespace roados::rvhl;
namespace fs = std::filesystem;

namespace {
const Library& lib() {
    static const Library l = *Library::load(fs::path(ROADOS_ASSETS_DIR) / "rvhl");
    return l;
}
int inkPixels(const Image& img) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) n += img.pixel(x, y).a > 0;
    return n;
}
}  // namespace

TEST(HazardRender, EveryEntryRendersInBothThemes) {
    ASSERT_FALSE(lib().empty());
    for (const auto& e : lib().entries()) {
        for (auto theme : {Theme::Dark, Theme::Light}) {
            Hazard h(lib(), e);
            h.theme(theme).size(64, 64);
            auto img = h.render();
            ASSERT_TRUE(img) << e.id << ": " << img.error().message;
            EXPECT_EQ(img->width(), 64);
            EXPECT_GT(inkPixels(*img), 64 * 64 / 10) << e.id;
        }
    }
}

TEST(HazardRender, CreateBySlugAndRenderAtSize) {
    auto h = Hazard::create(lib(), "large_pothole");
    ASSERT_TRUE(h) << h.error().message;
    h->severity(Severity::S5).distance(150).size(256, 256);
    auto img = h->render();
    ASSERT_TRUE(img) << img.error().message;
    EXPECT_EQ(img->width(), 256);
    ASSERT_TRUE(img->savePNG(fs::path(ROADOS_TEST_OUTPUT_DIR) / "hazard_rdm003_s5.png"));
    // The frame's top border (design y 5…11, left of the code tab) is the S5 colour (#D93636).
    const Color border = img->pixel(2 * 40, 2 * 9);
    EXPECT_EQ(border, Color("#D93636"));
    EXPECT_EQ(img->pixel(0, 0).a, 0) << "outside the tile is transparent";
}

TEST(HazardRender, OverlaysUseTheDesignSpace) {
    auto h = Hazard::create(lib(), "RDM-003");
    ASSERT_TRUE(h);
    h->size(256, 128).overlay(Circle().center(64, 64).radius(8).fill(Color("#00FF00")));
    auto img = h->render();
    ASSERT_TRUE(img);
    // 128-unit icon scaled ×1 and centred horizontally in 256 px → (64,64) maps to (128,64).
    EXPECT_EQ(img->pixel(128, 64), Color("#00FF00"));
    auto svg = h->toSvg();
    ASSERT_TRUE(svg);
    EXPECT_NE(svg->find("<svg"), std::string::npos);
}

TEST(HazardRender, RejectsInvalidSize) {
    auto h = Hazard::create(lib(), "RDM-003");
    ASSERT_TRUE(h);
    h->size(0, 10);
    auto img = h->render();
    ASSERT_FALSE(img);
    EXPECT_EQ(img.error().code, ErrorCode::InvalidArgument);
}

TEST(HazardRender, UnknownNameExplainsCandidates) {
    auto h = Hazard::create(lib(), "pothole");
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code, ErrorCode::NotFound);
    EXPECT_NE(h.error().message.find("RDM-001"), std::string::npos);
}
