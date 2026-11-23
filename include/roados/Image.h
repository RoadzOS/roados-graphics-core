#pragma once
/// Immutable raster image. Pixels are stored as RGBA8888, **premultiplied**
/// alpha, row-major. That layout matches Android's ARGB_8888 Bitmap memory and
/// CoreGraphics' kCGImageAlphaPremultipliedLast, so mobile SDKs can wrap it
/// without conversion. Copying an Image is cheap: copies share the pixels.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

#include "roados/Color.h"
#include "roados/Result.h"

namespace roados {

class Image {
public:
    Image() = default;  ///< empty image (width = height = 0)

    /// Takes premultiplied RGBA8888 pixels. `stride` = bytes per row (0 → width*4).
    static Result<Image> fromPixels(int width, int height, std::span<const std::uint8_t> rgbaPremul,
                                    std::size_t stride = 0);
    /// Decodes an encoded image (PNG in this build).
    static Result<Image> decode(std::span<const std::uint8_t> encoded);
    static Result<Image> load(const std::filesystem::path& file);

    int width() const;
    int height() const;
    bool empty() const { return !impl_; }
    std::size_t stride() const;
    std::span<const std::uint8_t> pixels() const;

    /// Un-premultiplied colour at (x, y). Requires 0 <= x < width, 0 <= y < height.
    Color pixel(int x, int y) const;

    Result<std::vector<std::uint8_t>> encodePNG() const;
    Result<void> savePNG(const std::filesystem::path& file) const;

    struct Impl;
    explicit Image(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
    const Impl* impl() const { return impl_.get(); }

private:
    std::shared_ptr<const Impl> impl_;
};

}  // namespace roados
