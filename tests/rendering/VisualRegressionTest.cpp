// Visual regression: real RVHL pictograms rendered by roados (Skia) vs the
// same SVGs rendered by headless Chrome (what Studio users see).
// Fixtures: tests/fixtures/visual-fixtures.txt, references: tests/fixtures/chrome-ref.
// Tolerances are documented in docs/svg-compatibility/README.md.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "roados/Graphics.h"
#include "support/ImageCompare.h"

using namespace roados;
namespace fs = std::filesystem;

namespace {

struct Fixture {
    std::string rel;
    bool text;
};

// Per-pixel error = max channel difference (premultiplied), 0..255.
constexpr int kPixelTolerance = 16;
// Geometry-only pictograms: Chrome rasterises with Skia too, so output is near-identical.
constexpr double kGeometryMaxMean = 0.25;
constexpr double kGeometryMaxPctOver = 0.25;
// Text pictograms: placement matches (ink centroid within 0.3 px at 128 px), but
// Chrome's Skia build applies different text gamma/contrast (≈13% more glyph
// coverage), so glyph edges differ. Worst measured (Chrome 157, macOS 15):
// mean 2.654, 3.271% over tolerance (STOP, weight 900).
constexpr double kTextMaxMean = 3.0;
constexpr double kTextMaxPctOver = 4.0;

std::vector<Fixture> fixtures() {
    std::vector<Fixture> out;
    std::ifstream in(fs::path(ROADOS_FIXTURES_DIR) / "visual-fixtures.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        Fixture f;
        std::string cls;
        ss >> f.rel >> cls;
        f.text = cls == "text";
        out.push_back(f);
    }
    return out;
}

std::string refName(const std::string& rel) {
    std::string s = rel;
    for (int i = 0; i < 2; ++i) s.replace(s.find('/'), 1, "__");
    return s.substr(0, s.size() - 4) + ".png";
}

class ChromeParity : public ::testing::TestWithParam<Fixture> {};

}  // namespace

TEST_P(ChromeParity, MatchesChromeWithinTolerance) {
    const Fixture& f = GetParam();
#if !defined(__APPLE__)
    if (f.text) GTEST_SKIP() << "text references are macOS-specific (font availability)";
#endif
    auto ref = Image::load(fs::path(ROADOS_FIXTURES_DIR) / "chrome-ref" / refName(f.rel));
    ASSERT_TRUE(ref.ok()) << ref.error().message;
    auto doc = SvgDocument::load(fs::path(ROADOS_ASSETS_DIR) / "rvhl" / "pictograms" / f.rel);
    ASSERT_TRUE(doc.ok()) << doc.error().message;

    Canvas c(ref->width(), ref->height());
    c.svg(SvgShape(*doc).size(float(ref->width()), float(ref->height())));
    auto img = c.render();
    ASSERT_TRUE(img.ok());

    const auto d = test::compare(*ref, *img, kPixelTolerance);
    ASSERT_FALSE(d.sizeMismatch);
    RecordProperty("mean", std::to_string(d.mean));
    RecordProperty("pct_over", std::to_string(d.pctOver));
    if (f.text) {
        EXPECT_LE(d.mean, kTextMaxMean) << f.rel;
        EXPECT_LE(d.pctOver, kTextMaxPctOver) << f.rel;
    } else {
        EXPECT_LE(d.mean, kGeometryMaxMean) << f.rel;
        EXPECT_LE(d.pctOver, kGeometryMaxPctOver) << f.rel;
    }
    std::printf("    %-62s mean=%.3f max=%3d over=%.3f%%\n", f.rel.c_str(), d.mean, d.max, d.pctOver);
    if (HasFailure()) {
        (void)img->savePNG(fs::path(ROADOS_TEST_OUTPUT_DIR) / ("FAIL__" + refName(f.rel)));
    }
}

INSTANTIATE_TEST_SUITE_P(Rvhl, ChromeParity, ::testing::ValuesIn(fixtures()),
                         [](const ::testing::TestParamInfo<Fixture>& i) {
                             std::string n = refName(i.param.rel);
                             for (auto& ch : n) if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
                             return n;
                         });
