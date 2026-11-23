#pragma once
/// Fill and stroke description: brushes (solid colour or gradient), stroke
/// geometry, opacity and blend mode.

#include <optional>
#include <variant>
#include <vector>

#include "roados/Color.h"
#include "roados/Geometry.h"

namespace roados {

struct GradientStop {
    float offset;  ///< 0..1
    Color color;
};

enum class SpreadMode { Pad, Repeat, Reflect };

struct LinearGradient {
    Point start;
    Point end;
    std::vector<GradientStop> stops;
    SpreadMode spread = SpreadMode::Pad;
};

struct RadialGradient {
    Point center;
    float radius = 0;
    std::vector<GradientStop> stops;
    SpreadMode spread = SpreadMode::Pad;
};

/// What a fill or stroke is painted with. Gradient coordinates are in the
/// shape's local coordinate system.
using Brush = std::variant<Color, LinearGradient, RadialGradient>;

enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };

struct Stroke {
    Brush brush = Color::Black();
    float width = 1;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    float miterLimit = 4;
    std::vector<float> dash;  ///< on/off lengths; an odd count is repeated (SVG semantics)
    float dashOffset = 0;
};

enum class BlendMode {
    SrcOver, Src, Clear, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn,
    HardLight, SoftLight, Difference, Exclusion, Plus, Hue, Saturation, Color, Luminosity,
};

struct Style {
    std::optional<Brush> fill;
    std::optional<Stroke> stroke;
    float opacity = 1;  ///< applied to the composited fill+stroke, as SVG `opacity`
    BlendMode blend = BlendMode::SrcOver;
    bool antiAlias = true;
};

}  // namespace roados
