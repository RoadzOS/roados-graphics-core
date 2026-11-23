#include <gtest/gtest.h>

#include "roados/Path.h"

using namespace roados;
using V = Path::Verb;

namespace roados {
// Readable failure output for EXPECT_EQ on paths.
void PrintTo(const Path& p, std::ostream* os) { *os << "Path(\"" << p.toSvg() << "\")"; }
}  // namespace roados

namespace {
void expectRect(const Rect& r, float x, float y, float w, float h, float eps = 1e-3f) {
    EXPECT_NEAR(r.x, x, eps);
    EXPECT_NEAR(r.y, y, eps);
    EXPECT_NEAR(r.width, w, eps);
    EXPECT_NEAR(r.height, h, eps);
}
}  // namespace

TEST(Path, BuilderRecordsVerbsAndPoints) {
    Path p;
    p.moveTo(0, 0).lineTo(10, 0).quadTo(15, 5, 10, 10).cubicTo(5, 15, 0, 15, 0, 10).close();
    EXPECT_EQ(p.verbs(), (std::vector<V>{V::Move, V::Line, V::Quad, V::Cubic, V::Close}));
    EXPECT_EQ(p.points().size(), 1u + 1 + 2 + 3);
}

TEST(Path, ImplicitMoveFollowsSvgSemantics) {
    Path p;
    p.lineTo(5, 5);  // no current point: starts at (0,0)
    EXPECT_EQ(p.verbs().front(), V::Move);
    EXPECT_EQ(p.points().front(), (Point{0, 0}));

    Path q;
    q.moveTo(3, 4).lineTo(10, 4).close().lineTo(3, 10);  // after close: restart at contour start
    EXPECT_EQ(q.verbs(), (std::vector<V>{V::Move, V::Line, V::Close, V::Move, V::Line}));
    EXPECT_EQ(q.points()[2], (Point{3, 4}));
}

TEST(Path, ParsesRvhlPathData) {
    // Real data from the RVHL glyph helpers (carSide) and compose.ts frames.
    const char* samples[] = {
        "M7 41V34c0-2 1.5-3.5 3.5-4l8-1.5 6.5-8.5h15l8 8.5 6 1c2 .5 3 2 3 4V41z",
        "M18 100V46Q18 8 64 8T110 46V100Z",
        "M4 8h56v50h-9V36a19 17 0 0 0-38 0v22H4z",
        "M5 16a2 2 0 0 1 2-2h46c3 0 6 3 6 7v19a2 2 0 0 1-2 2H5z",
        "M24 60c0-12 16-14 16-24s-16-12-16-22",
        "M8 58h8M24 58h8M40 58h8",
    };
    for (const char* d : samples) {
        auto p = Path::fromSvg(d);
        ASSERT_TRUE(p.ok()) << d;
        EXPECT_FALSE(p->isEmpty()) << d;
    }
}

TEST(Path, ArcsBecomeConics) {
    auto p = Path::fromSvg("M10 50 A40 40 0 0 1 90 50");
    ASSERT_TRUE(p.ok());
    bool hasConic = false;
    for (auto v : p->verbs()) hasConic |= v == V::Conic;
    EXPECT_TRUE(hasConic);
    // Half circle of radius 40 above the chord (sweep=1 → clockwise in y-down space).
    expectRect(p->bounds(), 10, 10, 80, 40);
}

TEST(Path, BuilderArcMatchesParsedArc) {
    Path built;
    built.moveTo(10, 50).arcTo(40, 40, 0, false, true, 90, 50);
    auto parsed = Path::fromSvg("M10 50 A40 40 0 0 1 90 50");
    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(built, *parsed);
}

TEST(Path, RejectsInvalidPathData) {
    auto r = Path::fromSvg("M 10 10 L foo");
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::Parse);
}

TEST(Path, TightBounds) {
    expectRect(Path().addCircle(50, 40, 10).bounds(), 40, 30, 20, 20);
    // Bounds use curve extrema, not control points.
    expectRect(Path().moveTo(0, 0).quadTo(50, 100, 100, 0).bounds(), 0, 0, 100, 50);
}

TEST(Path, SvgRoundTrip) {
    Path p;
    p.moveTo(1, 2).lineTo(30, 2).cubicTo(40, 10, 40, 20, 30, 28).close();
    auto back = Path::fromSvg(p.toSvg());
    ASSERT_TRUE(back.ok());
    EXPECT_EQ(*back, p);
}

TEST(Path, TransformedMapsAllPoints) {
    const Path p = Path().addRect({0, 0, 10, 10}).transformed(Transform::Translate(5, 5).scale(2));
    expectRect(p.bounds(), 5, 5, 20, 20);
}

TEST(Path, TransformKeepsFillRule) {
    auto p = Path::fromSvg("M0 0h10v10h-10z");
    ASSERT_TRUE(p.ok());
    p->setFillRule(FillRule::EvenOdd);
    EXPECT_EQ(p->transformed(Transform::Scale(2)).fillRule(), FillRule::EvenOdd);
}
