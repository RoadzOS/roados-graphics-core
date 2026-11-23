#pragma once
#include <cstdint>
#include <vector>

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "roados/Image.h"

namespace roados {

struct Image::Impl {
    int width = 0;
    int height = 0;
    std::size_t stride = 0;
    std::vector<std::uint8_t> pixels;  // RGBA8888 premultiplied

    SkImageInfo info() const {
        return SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
    }
    SkPixmap pixmap() const { return SkPixmap(info(), pixels.data(), stride); }
};

}  // namespace roados
