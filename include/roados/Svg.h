#pragma once
/// SVG documents rendered by Skia's SVG module, plus a scene node to place them.
///
/// Before parsing, SvgDocument applies a documented *text normalisation* that
/// fills two gaps in Skia's SVG module (m158). It never touches path data,
/// shapes, colours or transforms:
///   1. font-family lists ("Inter, 'Segoe UI', Arial, sans-serif") are resolved
///      to the first installed family. Skia would treat the whole list as one
///      family name and fall back to the system default.
///   2. dominant-baseline="central" on <text> is converted to an equivalent `dy`
///      of (ascent − descent) / 2 for the resolved font. Skia ignores
///      dominant-baseline, so text would otherwise sit on the alphabetic baseline.
/// Every adjustment is counted in SvgReport. Anything that cannot be normalised
/// is recorded as a warning; nothing is silently dropped.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "roados/Geometry.h"
#include "roados/Renderer.h"
#include "roados/Result.h"
#include "roados/Shape.h"

namespace roados {

struct SvgOptions {
    bool normalizeText = true;
};

struct SvgReport {
    int textElements = 0;
    int baselinesAdjusted = 0;
    int fontFamiliesResolved = 0;
    std::vector<std::string> warnings;
};

class SvgDocument {
public:
    SvgDocument() = default;

    /// Parses SVG markup. Fonts for text normalisation and text rendering come from `renderer`.
    static Result<SvgDocument> parse(std::string_view svg, const SvgOptions& options = {},
                                     const Renderer& renderer = Renderer::shared());
    static Result<SvgDocument> load(const std::filesystem::path& file, const SvgOptions& options = {},
                                    const Renderer& renderer = Renderer::shared());

    bool empty() const { return !impl_; }
    /// Size from the root width/height (falls back to the viewBox size).
    Size intrinsicSize() const;
    std::optional<Rect> viewBox() const;
    const SvgReport& report() const;
    /// The markup actually handed to Skia (after normalisation).
    const std::string& normalizedSource() const;

    struct Impl;
    const Impl* impl() const { return impl_.get(); }

private:
    explicit SvgDocument(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
    std::shared_ptr<const Impl> impl_;
};

enum class AspectFit {
    Meet,     ///< scale uniformly to fit inside the box, centred (SVG xMidYMid meet)
    Stretch,  ///< fill the box exactly (SVG preserveAspectRatio="none")
};

/// Places an SvgDocument into a destination box. Opacity, blend and transform apply.
class SvgShape : public Styled<SvgShape> {
public:
    SvgShape() = default;
    explicit SvgShape(SvgDocument doc) : doc_(std::move(doc)) {}
    SvgShape& document(SvgDocument doc) { doc_ = std::move(doc); return *this; }
    SvgShape& position(float x, float y) { box_.x = x; box_.y = y; return *this; }
    /// Box size; 0 means "use the document's intrinsic size".
    SvgShape& size(float w, float h) { box_.width = w; box_.height = h; return *this; }
    SvgShape& fit(AspectFit f) { fit_ = f; return *this; }

    const SvgDocument& document() const { return doc_; }
    Rect box() const;
    AspectFit fit() const { return fit_; }
    /// Transform mapping document user space into the box.
    Transform placement() const;

private:
    SvgDocument doc_;
    Rect box_;
    AspectFit fit_ = AspectFit::Meet;
};

}  // namespace roados
