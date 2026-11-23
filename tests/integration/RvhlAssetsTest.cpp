// Every pictogram in the generated RVHL bundle must parse, normalise without
// warnings and render visible ink through Skia.
#include <gtest/gtest.h>

#include <atomic>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "roados/Graphics.h"

using namespace roados;
namespace fs = std::filesystem;

namespace {
std::vector<fs::path> allPictograms() {
    std::vector<fs::path> out;
    const fs::path root = fs::path(ROADOS_ASSETS_DIR) / "rvhl" / "pictograms";
    if (!fs::exists(root)) return out;
    for (const auto& e : fs::recursive_directory_iterator(root))
        if (e.is_regular_file() && e.path().extension() == ".svg") out.push_back(e.path());
    std::sort(out.begin(), out.end());
    return out;
}
}  // namespace

TEST(RvhlAssets, BundleIsPresent) {
    // 762 = every (entry, frame, theme, variant) the Studio composer can produce (Studio f6b145e).
    EXPECT_EQ(allPictograms().size(), 762u);
}

TEST(RvhlAssets, EveryPictogramParsesAndRenders) {
    const auto files = allPictograms();
    ASSERT_FALSE(files.empty());
    int texts = 0, adjusted = 0, intentionallyEmpty = 0;
    for (const auto& f : files) {
        std::ifstream in(f);
        const std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        // Some glyphs draw nothing inside their convention frame because the frame
        // *is* the sign (e.g. give_way: `standard ? '' : ...` in the Studio).
        const bool drawsSomething = src.find("<path") != std::string::npos || src.find("<rect") != std::string::npos ||
                                    src.find("<circle") != std::string::npos || src.find("<text") != std::string::npos;
        auto doc = SvgDocument::load(f);
        ASSERT_TRUE(doc.ok()) << f << ": " << doc.error().message;
        EXPECT_EQ(doc->intrinsicSize(), (Size{64, 64})) << f;
        EXPECT_TRUE(doc->report().warnings.empty()) << f << ": " << doc->report().warnings.front();
        texts += doc->report().textElements;
        adjusted += doc->report().baselinesAdjusted;

        Canvas c(64, 64);
        c.svg(SvgShape(*doc));
        auto img = c.render();
        ASSERT_TRUE(img.ok()) << f;
        int ink = 0;
        for (int y = 0; y < 64; y += 2)
            for (int x = 0; x < 64; x += 2) ink += img->pixel(x, y).a > 0;
        if (drawsSomething) EXPECT_GT(ink, 10) << f << " rendered (almost) nothing";
        else { EXPECT_EQ(ink, 0) << f; ++intentionallyEmpty; }
    }
    EXPECT_EQ(intentionallyEmpty, 2);  // rsg-002 give-way on give_way_triangle, dark + light
    // Every <text> in the catalogue uses dominant-baseline="central" (census) and must be adjusted.
    EXPECT_GT(texts, 0);
    EXPECT_EQ(adjusted, texts);
}

TEST(RvhlAssets, ConcurrentSvgRenderingIsDeterministic) {
    auto doc = SvgDocument::load(fs::path(ROADOS_ASSETS_DIR) / "rvhl/pictograms/prohibitory_circle/dark/rsg-012-weight-restriction.svg");
    ASSERT_TRUE(doc.ok());
    Canvas c(128, 128);
    c.svg(SvgShape(*doc).size(128, 128));
    const auto ref = c.render();
    ASSERT_TRUE(ref.ok());
    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&] {
            for (int i = 0; i < 20; ++i) {
                auto img = c.render();
                if (!img || !std::equal(img->pixels().begin(), img->pixels().end(), ref->pixels().begin())) ++mismatches;
            }
        });
    for (auto& th : threads) th.join();
    EXPECT_EQ(mismatches.load(), 0);
}
