#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkData.h"
#include "include/encode/SkPngEncoder.h"
#include "renderer/ImageImpl.h"

namespace roados {

int Image::width() const { return impl_ ? impl_->width : 0; }
int Image::height() const { return impl_ ? impl_->height : 0; }
std::size_t Image::stride() const { return impl_ ? impl_->stride : 0; }
std::span<const std::uint8_t> Image::pixels() const {
    if (!impl_) return {};
    return {impl_->pixels.data(), impl_->pixels.size()};
}

Result<Image> Image::fromPixels(int w, int h, std::span<const std::uint8_t> px, std::size_t stride) {
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535)
        return makeError(ErrorCode::InvalidArgument, "image dimensions must be 1..65535");
    const std::size_t row = std::size_t(w) * 4;
    if (stride == 0) stride = row;
    if (stride < row) return makeError(ErrorCode::InvalidArgument, "stride is smaller than width*4");
    if (px.size() < stride * std::size_t(h - 1) + row)
        return makeError(ErrorCode::InvalidArgument, "pixel buffer is too small for the given dimensions");
    auto impl = std::make_shared<Impl>();
    impl->width = w;
    impl->height = h;
    impl->stride = row;
    impl->pixels.resize(row * std::size_t(h));
    for (int y = 0; y < h; ++y) std::memcpy(impl->pixels.data() + row * y, px.data() + stride * y, row);
    return Image(std::move(impl));
}

Result<Image> Image::decode(std::span<const std::uint8_t> encoded) {
    if (encoded.empty()) return makeError(ErrorCode::InvalidArgument, "empty image data");
    if (!SkPngDecoder::IsPng(encoded.data(), encoded.size()))
        return makeError(ErrorCode::Unsupported, "only PNG decoding is enabled in this build");
    SkCodec::Result r;
    auto codec = SkPngDecoder::Decode(SkData::MakeWithCopy(encoded.data(), encoded.size()), &r);
    if (!codec) return makeError(ErrorCode::Parse, "PNG could not be decoded");
    const SkImageInfo src = codec->getInfo();
    auto impl = std::make_shared<Impl>();
    impl->width = src.width();
    impl->height = src.height();
    impl->stride = std::size_t(src.width()) * 4;
    impl->pixels.resize(impl->stride * std::size_t(src.height()));
    if (codec->getPixels(impl->info(), impl->pixels.data(), impl->stride) != SkCodec::kSuccess)
        return makeError(ErrorCode::Parse, "PNG pixel data is incomplete or corrupt");
    return Image(std::move(impl));
}

Result<Image> Image::load(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return makeError(ErrorCode::Io, "cannot open " + file.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return decode(bytes);
}

Color Image::pixel(int x, int y) const {
    const std::uint8_t* p = impl_->pixels.data() + impl_->stride * std::size_t(y) + std::size_t(x) * 4;
    const int a = p[3];
    if (a == 0) return Color::Transparent();
    auto un = [a](int c) { return std::uint8_t(std::min(255, int(std::lround(c * 255.0 / a)))); };
    return {un(p[0]), un(p[1]), un(p[2]), std::uint8_t(a)};
}

Result<std::vector<std::uint8_t>> Image::encodePNG() const {
    if (!impl_) return makeError(ErrorCode::InvalidArgument, "cannot encode an empty image");
    sk_sp<SkData> data = SkPngEncoder::Encode(impl_->pixmap(), SkPngEncoder::Options{});
    if (!data) return makeError(ErrorCode::Render, "PNG encoding failed");
    const auto* b = static_cast<const std::uint8_t*>(data->data());
    return std::vector<std::uint8_t>(b, b + data->size());
}

Result<void> Image::savePNG(const std::filesystem::path& file) const {
    auto png = encodePNG();
    if (!png) return png.error();
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return makeError(ErrorCode::Io, "cannot write " + file.string());
    out.write(reinterpret_cast<const char*>(png->data()), std::streamsize(png->size()));
    if (!out) return makeError(ErrorCode::Io, "write failed for " + file.string());
    return {};
}

}  // namespace roados
