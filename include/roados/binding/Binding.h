#pragma once
/// Binding facade for platform SDKs (Android JNI, iOS Objective-C++).
///
/// Platform bridges should only *marshal*: they copy their native values into
/// the plain structs below and call these functions. All interpretation
/// (styles, gradients, transforms, scene building, hazard options) happens here,
/// once, in tested C++, so both SDKs behave identically and neither
/// re-implements rendering logic.
///
/// Colours are 0xAARRGGBB integers (Android's convention). Enum-like fields are
/// plain ints whose values are documented next to each field; out-of-range
/// values are rejected with ErrorCode::InvalidArgument, never clamped silently.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "roados/Canvas.h"
#include "roados/rvhl/Hazard.h"

namespace roados::binding {

/// Library and binding version (same as the core).
std::string versionString();

// ------------------------------------------------------------------ style

struct PaintSpec {
    int kind = 0;               ///< 0 none, 1 solid, 2 linear gradient, 3 radial gradient
    std::uint32_t argb = 0xFF000000;
    float x0 = 0, y0 = 0;       ///< linear: start · radial: centre
    float x1 = 0, y1 = 0;       ///< linear: end
    float radius = 0;           ///< radial radius
    std::vector<float> stopOffsets;        ///< 0…1, same length as stopColors
    std::vector<std::uint32_t> stopColors;
    int spread = 0;             ///< 0 pad, 1 repeat, 2 reflect
};

struct StyleSpec {
    PaintSpec fill;
    PaintSpec stroke;
    float strokeWidth = 1;
    int lineCap = 0;   ///< 0 butt, 1 round, 2 square
    int lineJoin = 0;  ///< 0 miter, 1 round, 2 bevel
    float miterLimit = 4;
    std::vector<float> dash;
    float dashOffset = 0;
    float opacity = 1;
    int blend = 0;     ///< roados::BlendMode order (0 = SrcOver)
    float transform[6] = {1, 0, 0, 1, 0, 0};  ///< a b c d e f (SVG matrix order)
};

Result<Style> toStyle(const StyleSpec& spec);
Transform toTransform(const float m[6]);

/// Compact encoding used by JNI (two primitive arrays instead of ~30 arguments).
/// Layout, version 1:
///   ints:   [1(version), fill.kind, fill.argb, fill.spread, stroke.kind, stroke.argb, stroke.spread,
///            lineCap, lineJoin, blend, nFillStops, nStrokeStops, nDash,
///            fillStopColors…, strokeStopColors…]
///   floats: [strokeWidth, miterLimit, dashOffset, opacity, a, b, c, d, e, f,
///            fill.x0, fill.y0, fill.x1, fill.y1, fill.radius,
///            stroke.x0, stroke.y0, stroke.x1, stroke.y1, stroke.radius,
///            fillStopOffsets…, strokeStopOffsets…, dash…]
Result<StyleSpec> decodeStyle(std::span<const std::int32_t> ints, std::span<const float> floats);

// ------------------------------------------------------------------ hazard

struct HazardSpec {
    std::string id;              ///< element ID ("RDM-003") or unambiguous slug
    std::string variant;         ///< empty = entry default
    int severity = 0;            ///< 0 = entry default, 1…5 = S1…S5
    int theme = 0;               ///< 0 dark, 1 light
    int representation = 0;      ///< 0 auto, 1 standard, 2 roados
    int sizeVariant = -1;        ///< -1 none, 0 S, 1 M, 2 L, 3 XL
    int count = 0;               ///< drawn when > 1
    int speedKind = -1;          ///< -1 none, 0 posted, 1 advisory, 2 unknown
    double speedKmh = -1;        ///< < 0 = no value
    int speedUnit = 0;           ///< 0 km/h, 1 mph
    double distanceM = -1;       ///< < 0 = none
    int unitSystem = 0;          ///< 0 metric, 1 imperial
    int direction = -1;          ///< -1 none, 0 left, 1 ahead, 2 right, 3 both
    int laneCount = 0;           ///< 0 = no lane chip
    int laneIndex = 0;
    bool showSeverityCode = true;
    bool hasBackground = false;
    std::uint32_t backgroundArgb = 0;
    std::vector<int> layers;     ///< rvhl::Layer values in paint order; empty = default
    int width = 128, height = 128;
};

Result<rvhl::HazardOptions> toHazardOptions(const HazardSpec& spec);
Result<rvhl::Hazard> makeHazard(const rvhl::Library& library, const HazardSpec& spec);

/// Loads (once) and returns the RVHL library at `bundleDir`. Later calls with
/// the same path return the cached instance. Thread-safe.
Result<rvhl::Library> sharedLibrary(const std::filesystem::path& bundleDir);

// ------------------------------------------------------------------ scene

/// Imperative scene builder over Canvas with a group stack, convenient for
/// bridges. Not thread-safe while building; render() is const.
class SceneBuilder {
public:
    SceneBuilder(int width, int height);

    int width() const { return canvas_.width(); }
    int height() const { return canvas_.height(); }
    void setBackground(std::uint32_t argb);

    Result<void> circle(float cx, float cy, float r, const StyleSpec& s);
    Result<void> ellipse(float cx, float cy, float rx, float ry, const StyleSpec& s);
    Result<void> rect(float x, float y, float w, float h, float rx, float ry, const StyleSpec& s);
    Result<void> line(float x1, float y1, float x2, float y2, const StyleSpec& s);
    /// xy = x0, y0, x1, y1, … (closed → polygon, open → polyline)
    Result<void> poly(std::span<const float> xy, bool closed, const StyleSpec& s);
    /// SVG path data ("M10 10 C …"). `evenOdd` selects the fill rule.
    Result<void> path(std::string_view svgPathData, bool evenOdd, const StyleSpec& s);
    /// anchor: 0 start, 1 middle, 2 end · baseline: 0 alphabetic, 1 central, 2 top, 3 bottom
    Result<void> text(std::string_view utf8, float x, float y, std::string_view cssFamilies, float size, int weight,
                      bool italic, int anchor, int baseline, const StyleSpec& s);
    /// SVG markup placed into a box (w/h 0 = intrinsic size), aspect-fit "meet".
    Result<void> svg(std::string_view markup, float x, float y, float w, float h, const StyleSpec& s);
    /// Premultiplied RGBA8888 pixels drawn into a box (dw/dh 0 = natural size).
    Result<void> image(std::span<const std::uint8_t> rgbaPremul, int width, int height, std::size_t stride, float x,
                       float y, float dw, float dh, const StyleSpec& s);
    /// A composed RVHL hazard icon placed into a box.
    Result<void> hazard(const rvhl::Library& library, const HazardSpec& spec, float x, float y, float w, float h);

    /// Opens a group; following draws go into it until endGroup().
    Result<void> beginGroup(const float transform[6], float opacity, bool clip, float cx, float cy, float cw, float ch);
    Result<void> endGroup();
    int groupDepth() const { return static_cast<int>(stack_.size()); }

    void clear();
    /// Closes any open groups into the scene (render/toSvg call this on a copy).
    const Canvas& canvas() const { return canvas_; }

    Result<Image> render(float scale = 1) const;
    Result<void> renderInto(std::span<std::uint8_t> rgbaPremul, int widthPx, int heightPx, std::size_t stride,
                            float scale = 1) const;
    Result<std::string> toSvg() const;

private:
    void add(Node n);
    Canvas finished() const;
    Canvas canvas_;
    std::vector<Group> stack_;
};

}  // namespace roados::binding
