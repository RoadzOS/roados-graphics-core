#pragma once
// Image comparison used by visual-regression tests.
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "roados/Image.h"

namespace roados::test {

struct DiffStats {
    double mean = 0;     ///< mean of per-pixel max channel error (0..255)
    int max = 0;         ///< largest channel error
    double pctOver = 0;  ///< % of pixels whose error exceeds the tolerance
    bool sizeMismatch = false;
};

/// Errors are measured on premultiplied values, so colour hidden under alpha=0 is ignored.
inline DiffStats compare(const Image& a, const Image& b, int tolerance) {
    DiffStats s;
    if (a.width() != b.width() || a.height() != b.height()) { s.sizeMismatch = true; return s; }
    const auto pa = a.pixels(), pb = b.pixels();
    long long sum = 0, over = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const auto* p = &pa[std::size_t(y) * a.stride() + std::size_t(x) * 4];
            const auto* q = &pb[std::size_t(y) * b.stride() + std::size_t(x) * 4];
            int e = 0;
            for (int c = 0; c < 4; ++c) e = std::max(e, std::abs(int(p[c]) - int(q[c])));
            sum += e;
            s.max = std::max(s.max, e);
            if (e > tolerance) ++over;
        }
    const double n = double(a.width()) * a.height();
    s.mean = double(sum) / n;
    s.pctOver = 100.0 * double(over) / n;
    return s;
}

}  // namespace roados::test
