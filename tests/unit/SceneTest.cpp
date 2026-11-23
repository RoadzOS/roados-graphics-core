#include <gtest/gtest.h>

#include "roados/Canvas.h"

using namespace roados;

TEST(Scene, ChildrenKeepInsertionOrder) {
    Group g;
    g.add(Circle()).add(Rectangle()).add(Text("x"));
    ASSERT_EQ(g.size(), 3u);
    EXPECT_TRUE(g.children()[0].is<Circle>());
    EXPECT_TRUE(g.children()[1].is<Rectangle>());
    EXPECT_TRUE(g.children()[2].is<Text>());
}

TEST(Scene, ReorderAndRemoveLayers) {
    Group g;
    g.add(Group("background")).add(Group("frame")).add(Group("pictogram"));
    ASSERT_TRUE(g.move(2, 0));
    EXPECT_EQ(g.children()[0].as<Group>()->name(), "pictogram");
    EXPECT_EQ(g.children()[1].as<Group>()->name(), "background");
    EXPECT_FALSE(g.move(0, 3));
    ASSERT_TRUE(g.remove(1));
    EXPECT_EQ(g.size(), 2u);
    EXPECT_FALSE(g.remove(5));
}

TEST(Scene, FindNamedGroup) {
    Group g;
    g.add(Group("frame").opacity(0.5f)).add(Circle());
    Group* f = g.findGroup("frame");
    ASSERT_NE(f, nullptr);
    EXPECT_FLOAT_EQ(f->opacity(), 0.5f);
    f->visible(false);
    EXPECT_FALSE(g.findGroup("frame")->visible());
    EXPECT_EQ(g.findGroup("missing"), nullptr);
}

TEST(Scene, NestedGroupsAreValues) {
    Group inner("inner");
    inner.add(Circle().radius(3));
    Group outer("outer");
    outer.add(inner);
    inner.add(Circle());  // modifying the original does not affect the copy
    EXPECT_EQ(outer.findGroup("inner")->size(), 1u);
}

TEST(Canvas, Configuration) {
    Canvas c(512, 256);
    EXPECT_EQ(c.width(), 512);
    EXPECT_EQ(c.height(), 256);
    EXPECT_EQ(c.background(), Color::Transparent());
    c.setBackground(Color("#0B1118")).circle(Circle()).line(Line());
    EXPECT_EQ(c.background(), Color("#0B1118"));
    EXPECT_EQ(c.root().size(), 2u);
    c.clear();
    EXPECT_EQ(c.root().size(), 0u);
}

TEST(Font, ParsesCssFamilyLists) {
    EXPECT_EQ(Font::parseFamilyList("Inter, 'Segoe UI', Roboto, Arial, sans-serif"),
              (std::vector<std::string>{"Inter", "Segoe UI", "Roboto", "Arial", "sans-serif"}));
    EXPECT_EQ(Font::parseFamilyList("'DIN Alternate', \"Barlow Condensed\", Arial Narrow"),
              (std::vector<std::string>{"DIN Alternate", "Barlow Condensed", "Arial Narrow"}));
    EXPECT_EQ(Font::parseFamilyList("Arial"), (std::vector<std::string>{"Arial"}));
    EXPECT_TRUE(Font::parseFamilyList(" , ").empty());
}
