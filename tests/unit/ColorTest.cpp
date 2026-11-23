#include <gtest/gtest.h>

#include "roados/Color.h"

using roados::Color;
using roados::ErrorCode;

// Compile-time validated literals.
static_assert(Color("#2196F3") == Color(0x21, 0x96, 0xF3, 255));
static_assert(Color("#fff") == Color(255, 255, 255, 255));
static_assert(Color("#0008") == Color(0, 0, 0, 0x88));
static_assert(Color("#11223344").a == 0x44);
static_assert(Color::rgb(0xE35D32) == Color("#E35D32"));
static_assert(Color::rgba(0xE35D3280) == Color("#E35D3280"));
static_assert(Color("#D93636").argb() == 0xFFD93636u);

TEST(Color, ParsesAllHexForms) {
    EXPECT_EQ(*Color::parse("#8296A5"), Color(0x82, 0x96, 0xA5));
    EXPECT_EQ(*Color::parse("#abc"), Color(0xAA, 0xBB, 0xCC));
    EXPECT_EQ(*Color::parse("#abcd"), Color(0xAA, 0xBB, 0xCC, 0xDD));
    EXPECT_EQ(*Color::parse("#0B111880"), Color(0x0B, 0x11, 0x18, 0x80));
    EXPECT_EQ(*Color::parse("  #E2C34C \n"), Color(0xE2, 0xC3, 0x4C));
    EXPECT_EQ(*Color::parse("transparent"), Color::Transparent());
}

TEST(Color, RejectsMalformedInput) {
    for (const char* bad : {"", "#", "E35D32", "#12", "#12345", "#1234567", "#GG0000", "red", "#E35D32 x"}) {
        auto r = Color::parse(bad);
        ASSERT_FALSE(r.ok()) << bad;
        EXPECT_EQ(r.error().code, ErrorCode::Parse);
    }
}

TEST(Color, FormatsHex) {
    EXPECT_EQ(Color("#e35d32").toHex(), "#E35D32");
    EXPECT_EQ(Color(1, 2, 3, 4).toHex(), "#01020304");
}

TEST(Color, RoundTripsThroughHex) {
    for (std::uint32_t v : {0x000000u, 0xFFFFFFu, 0x8296A5u, 0xD93636u, 0x123456u}) {
        const Color c = Color::rgb(v);
        EXPECT_EQ(*Color::parse(c.toHex()), c);
    }
}

TEST(Color, OpacityMultipliesAlpha) {
    EXPECT_EQ(Color("#FF0000").withOpacity(0.5f).a, 128);
    EXPECT_EQ(Color("#FF000080").withOpacity(0.5f).a, 64);
    EXPECT_EQ(Color("#FF0000").withOpacity(2.0f).a, 255);
    EXPECT_EQ(Color("#FF0000").withOpacity(-1.0f).a, 0);
}
