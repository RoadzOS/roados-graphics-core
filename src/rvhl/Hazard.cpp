#include "roados/rvhl/Hazard.h"

#include <algorithm>

namespace roados::rvhl {

Result<Hazard> Hazard::create(const Library& library, std::string_view idOrSlug) {
    auto entry = library.get(idOrSlug);
    if (!entry) return entry.error();
    return Hazard(library, **entry);
}

Hazard::Hazard(Library library, const Entry& entry) : library_(std::move(library)), entry_(&entry) {}

Result<ComposedHazard> Hazard::compose() const { return rvhl::compose(library_, *entry_, options_); }

Result<Canvas> Hazard::toCanvas() const {
    if (width_ <= 0 || height_ <= 0 || width_ > kMaxRenderDimension || height_ > kMaxRenderDimension)
        return makeError(ErrorCode::InvalidArgument,
                         "hazard size must be 1…" + std::to_string(kMaxRenderDimension) + " px, got " +
                             std::to_string(width_) + "x" + std::to_string(height_));
    auto composed = compose();
    if (!composed) return composed.error();
    auto doc = SvgDocument::parse(composed->svg);
    if (!doc) return doc.error();

    Canvas canvas(width_, height_);
    canvas.svg(SvgShape(std::move(*doc)).position(0, 0).size(static_cast<float>(width_), static_cast<float>(height_)));
    if (!overlays_.empty()) {
        // Same uniform, centred placement as the SVG (xMidYMid meet) so overlays
        // line up with the 128×128 design space.
        const float s = static_cast<float>(std::min(width_, height_)) / 128.0f;
        const float ox = (static_cast<float>(width_) - 128.0f * s) / 2;
        const float oy = (static_cast<float>(height_) - 128.0f * s) / 2;
        Group g("overlays");
        g.transform(Transform::Translate(ox, oy) * Transform::Scale(s));
        for (const auto& n : overlays_) g.add(n);
        canvas.group(std::move(g));
    }
    return canvas;
}

Result<Image> Hazard::render(const RenderOptions& options) const {
    auto canvas = toCanvas();
    if (!canvas) return canvas.error();
    return canvas->render(options);
}

Result<std::string> Hazard::toSvg() const {
    if (overlays_.empty()) {
        auto composed = compose();
        if (!composed) return composed.error();
        return std::move(composed->svg);
    }
    auto canvas = toCanvas();
    if (!canvas) return canvas.error();
    return canvas->toSvg();
}

}  // namespace roados::rvhl
