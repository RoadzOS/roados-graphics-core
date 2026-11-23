// Compares two PNGs and writes a side-by-side sheet: reference | candidate | diff.
//   image_diff <reference.png> <candidate.png> [sheet.png] [--tolerance N]
// Prints: mean absolute error per channel (0..255, over RGBA), max channel error,
// and the share of pixels whose max channel error exceeds the tolerance.
// Exit code: 0 always for comparisons (decisions are made by tests), 1 on I/O errors.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <roados/Graphics.h>

using namespace roados;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <reference.png> <candidate.png> [sheet.png] [--tolerance N]\n", argv[0]);
        return 2;
    }
    int tolerance = 16;
    std::string sheet;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--tolerance") && i + 1 < argc) tolerance = std::atoi(argv[++i]);
        else sheet = argv[i];
    }
    auto a = Image::load(argv[1]);
    auto b = Image::load(argv[2]);
    if (!a || !b) {
        std::fprintf(stderr, "error: %s\n", (!a ? a.error() : b.error()).message.c_str());
        return 1;
    }
    if (a->width() != b->width() || a->height() != b->height()) {
        std::printf("size mismatch: %dx%d vs %dx%d\n", a->width(), a->height(), b->width(), b->height());
        return 0;
    }
    const int w = a->width(), h = a->height();
    double sum = 0;
    int maxErr = 0, over = 0;
    std::vector<std::uint8_t> diff(std::size_t(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Color p = a->pixel(x, y), q = b->pixel(x, y);
            // Compare premultiplied-equivalent values so invisible colour under alpha=0 doesn't count.
            auto pm = [](Color c, int ch) { return (int(ch) * c.a + 127) / 255; };
            const int e = std::max({std::abs(pm(p, p.r) - pm(q, q.r)), std::abs(pm(p, p.g) - pm(q, q.g)),
                                    std::abs(pm(p, p.b) - pm(q, q.b)), std::abs(int(p.a) - q.a)});
            sum += e;
            maxErr = std::max(maxErr, e);
            if (e > tolerance) ++over;
            const std::uint8_t v = std::uint8_t(std::min(255, e * 4));
            std::uint8_t* d = &diff[(std::size_t(y) * w + x) * 4];
            d[0] = v; d[1] = 0; d[2] = std::uint8_t(e ? 0 : 40); d[3] = 255;
        }
    const double pctOver = 100.0 * over / (double(w) * h);
    std::printf("mean=%.3f max=%d over_tol(%d)=%.3f%%\n", sum / (double(w) * h), maxErr, tolerance, pctOver);

    if (!sheet.empty()) {
        auto d = Image::fromPixels(w, h, diff);
        Canvas c(w * 3 + 40, h + 20);
        c.setBackground(Color("#C9D2DA"));
        c.image(ImageShape().image(*a).position(10, 10));
        c.image(ImageShape().image(*b).position(float(w + 20), 10));
        c.image(ImageShape().image(*d).position(float(2 * w + 30), 10));
        if (auto img = c.render(); img) (void)img->savePNG(sheet);
    }
    return 0;
}
