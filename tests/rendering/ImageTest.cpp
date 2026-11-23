#include <gtest/gtest.h>

#include <filesystem>

#include "roados/Graphics.h"

using namespace roados;

TEST(Image, EmptyByDefault) {
    Image img;
    EXPECT_TRUE(img.empty());
    EXPECT_EQ(img.width(), 0);
    EXPECT_TRUE(img.pixels().empty());
    EXPECT_FALSE(img.encodePNG().ok());
}

TEST(Image, FromPixelsValidatesInput) {
    std::vector<std::uint8_t> px(16);
    EXPECT_FALSE(Image::fromPixels(0, 1, px).ok());
    EXPECT_FALSE(Image::fromPixels(4, 2, px).ok());         // needs 32 bytes
    EXPECT_FALSE(Image::fromPixels(2, 2, px, 4).ok());      // stride < width*4
    EXPECT_TRUE(Image::fromPixels(2, 2, px).ok());
}

TEST(Image, PixelUnpremultiplies) {
    const std::vector<std::uint8_t> px{64, 0, 0, 128};  // premultiplied half-transparent red
    auto img = Image::fromPixels(1, 1, px);
    ASSERT_TRUE(img.ok());
    EXPECT_EQ(img->pixel(0, 0), Color(128, 0, 0, 128));
}

TEST(Image, PngRoundTripIsLossless) {
    Canvas c(33, 17);  // odd sizes exercise row handling
    c.setBackground(Color("#EEF1F4"));
    c.circle(Circle().center(16, 8).radius(6).fill(Color("#E35D32")));
    c.rectangle(Rectangle().position(25, 0).size(8, 17).fill(Color(0x1F, 0x4E, 0x9E, 160)));
    auto img = c.render();
    ASSERT_TRUE(img.ok());

    auto png = img->encodePNG();
    ASSERT_TRUE(png.ok());
    ASSERT_GT(png->size(), 8u);
    EXPECT_EQ((*png)[1], 'P');
    EXPECT_EQ((*png)[2], 'N');
    EXPECT_EQ((*png)[3], 'G');

    auto back = Image::decode(*png);
    ASSERT_TRUE(back.ok()) << back.error().message;
    ASSERT_EQ(back->width(), 33);
    ASSERT_EQ(back->height(), 17);
    for (int y = 0; y < 17; ++y)
        for (int x = 0; x < 33; ++x) {
            const Color a = img->pixel(x, y), b = back->pixel(x, y);
            // Premultiplied → PNG (straight) → premultiplied can move a channel by 1 at partial alpha.
            ASSERT_LE(std::abs(int(a.r) - b.r) + std::abs(int(a.g) - b.g) + std::abs(int(a.b) - b.b), 3) << x << "," << y;
            ASSERT_EQ(a.a, b.a) << x << "," << y;
        }
}

TEST(Image, SaveAndLoadFile) {
    Canvas c(8, 8);
    c.setBackground(Color("#0F6B3A"));
    auto img = c.render();
    ASSERT_TRUE(img.ok());
    const std::filesystem::path file = std::filesystem::path(ROADOS_TEST_OUTPUT_DIR) / "save_load.png";
    ASSERT_TRUE(img->savePNG(file).ok());
    auto loaded = Image::load(file);
    ASSERT_TRUE(loaded.ok());
    EXPECT_EQ(loaded->pixel(4, 4), Color("#0F6B3A"));

    auto missing = Image::load(std::filesystem::path(ROADOS_TEST_OUTPUT_DIR) / "does-not-exist.png");
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, ErrorCode::Io);
}

TEST(Image, DecodeRejectsNonPng) {
    const std::vector<std::uint8_t> junk{1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto r = Image::decode(junk);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
}
