#pragma once
/// Single-line text. Fonts are resolved by an ordered family list (as in CSS),
/// so a missing family falls back to the next one.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roados/Shape.h"

namespace roados {

enum class TextAnchor { Start, Middle, End };

/// Vertical alignment relative to the text's y coordinate.
enum class TextBaseline {
    Alphabetic,  ///< y is the alphabetic baseline (SVG default)
    Central,     ///< y is midway between ascent and descent (SVG dominant-baseline="central")
    Top,         ///< y is the font ascent line
    Bottom,      ///< y is the font descent line
};

struct Font {
    std::vector<std::string> families{"sans-serif"};
    float size = 16;
    int weight = 400;  ///< 100..900
    bool italic = false;

    /// Parses a CSS font-family list: "Inter, 'Segoe UI', Roboto, sans-serif".
    static std::vector<std::string> parseFamilyList(std::string_view css);
};

class Text : public Styled<Text> {
public:
    Text() = default;
    explicit Text(std::string utf8) : content_(std::move(utf8)) {}

    Text& content(std::string utf8) { content_ = std::move(utf8); return *this; }
    Text& position(float x, float y) { pos_ = {x, y}; return *this; }
    Text& font(Font f) { font_ = std::move(f); return *this; }
    Text& family(std::string_view cssFamilyList) { font_.families = Font::parseFamilyList(cssFamilyList); return *this; }
    Text& size(float px) { font_.size = px; return *this; }
    Text& weight(int w) { font_.weight = w; return *this; }
    Text& italic(bool on) { font_.italic = on; return *this; }
    Text& anchor(TextAnchor a) { anchor_ = a; return *this; }
    Text& baseline(TextBaseline b) { baseline_ = b; return *this; }

    const std::string& content() const { return content_; }
    Point position() const { return pos_; }
    const Font& font() const { return font_; }
    TextAnchor anchor() const { return anchor_; }
    TextBaseline baseline() const { return baseline_; }

private:
    std::string content_;
    Point pos_;
    Font font_;
    TextAnchor anchor_ = TextAnchor::Start;
    TextBaseline baseline_ = TextBaseline::Alphabetic;
};

}  // namespace roados
