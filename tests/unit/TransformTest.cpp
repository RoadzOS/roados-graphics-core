#include <gtest/gtest.h>

#include "roados/Transform.h"

using namespace roados;

namespace {
void expectNear(Point a, Point b, float eps = 1e-4f) {
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
}
}  // namespace

TEST(Transform, IdentityByDefault) {
    EXPECT_TRUE(Transform().isIdentity());
    EXPECT_EQ(Transform().map({3, 4}), (Point{3, 4}));
}

TEST(Transform, TranslateAndScale) {
    EXPECT_EQ(Transform::Translate(10, 20).map({1, 2}), (Point{11, 22}));
    EXPECT_EQ(Transform::Scale(2, 3).map({1, 2}), (Point{2, 6}));
}

TEST(Transform, RotationIsClockwiseInYDownSpace) {
    // +x rotates onto +y (screen-clockwise), exactly for multiples of 90°.
    EXPECT_EQ(Transform::Rotate(90).map({1, 0}), (Point{0, 1}));
    EXPECT_EQ(Transform::Rotate(180).map({1, 0}), (Point{-1, 0}));
    EXPECT_EQ(Transform::Rotate(-90).map({1, 0}), (Point{0, -1}));
    expectNear(Transform::Rotate(45).map({1, 0}), {0.70710678f, 0.70710678f});
}

TEST(Transform, RotateAboutCentre) {
    expectNear(Transform::Rotate(90, 64, 64).map({64, 0}), {128, 64});
}

TEST(Transform, ChainingFollowsSvgTransformListOrder) {
    // SVG "translate(10 0) rotate(90)": rotate first, then translate.
    const Transform t = Transform::Translate(10, 0).rotate(90);
    EXPECT_EQ(t.map({1, 0}), (Point{10, 1}));
    // SVG "translate(x y) scale(s)" as used by RVHL pictogram placement.
    const Transform place = Transform::Translate(31, 22).scale(1.03125f);
    expectNear(place.map({64, 64}), {31 + 66, 22 + 66});
}

TEST(Transform, MatrixMatchesSvgConvention) {
    // RVHL mirror(): matrix(-1 0 0 1 64 0)
    EXPECT_EQ(Transform::Matrix(-1, 0, 0, 1, 64, 0).map({10, 5}), (Point{54, 5}));
}

TEST(Transform, InverseUndoesTransform) {
    const Transform t = Transform::Translate(5, -3).rotate(33).scale(2, 0.5f);
    const auto inv = t.inverted();
    ASSERT_TRUE(inv.has_value());
    expectNear(inv->map(t.map({7, 11})), {7, 11});
    EXPECT_FALSE(Transform::Scale(0, 1).inverted().has_value());
}

TEST(Transform, MapRectReturnsAxisAlignedBounds) {
    const Rect r = Transform::Rotate(90).mapRect({0, 0, 10, 20});
    EXPECT_NEAR(r.x, -20, 1e-5);
    EXPECT_NEAR(r.y, 0, 1e-5);
    EXPECT_NEAR(r.width, 20, 1e-5);
    EXPECT_NEAR(r.height, 10, 1e-5);
}
