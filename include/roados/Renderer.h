#pragma once
/// The Renderer turns a Canvas scene into pixels or SVG, using Skia. It owns
/// the font manager. A Renderer is immutable after construction and safe to
/// use from several threads at once.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "roados/Color.h"
#include "roados/Image.h"
#include "roados/Result.h"
#include "roados/Text.h"

namespace roados {

class Canvas;

struct RenderOptions {
    /// Output pixels per canvas unit (2 → a 64×64 canvas renders to 128×128 px).
    float scale = 1;
    /// Overrides the canvas background when set.
    std::optional<Color> background;
};

struct TextMetrics {
    float width = 0;    ///< advance width
    float ascent = 0;   ///< distance above baseline (positive)
    float descent = 0;  ///< distance below baseline (positive)
    std::string resolvedFamily;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Process-wide default instance (created on first use, thread-safe).
    static const Renderer& shared();

    Result<Image> render(const Canvas& canvas, const RenderOptions& options = {}) const;

    /// Renders into caller-owned premultiplied RGBA8888 memory (e.g. an Android
    /// Bitmap or a CVPixelBuffer). The buffer must hold height*stride bytes.
    Result<void> renderInto(const Canvas& canvas, std::span<std::uint8_t> rgbaPremul, int widthPx, int heightPx,
                            std::size_t stride, const RenderOptions& options = {}) const;

    Result<std::string> renderSvg(const Canvas& canvas) const;

    TextMetrics measure(const Text& text) const;

    struct Impl;
    const Impl& impl() const { return *impl_; }

private:
    std::unique_ptr<Impl> impl_;
};

/// Hard limit on output size, guarding against accidental huge allocations.
inline constexpr int kMaxRenderDimension = 16384;

}  // namespace roados
