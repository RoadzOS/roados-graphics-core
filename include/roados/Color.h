#pragma once
/// 8-bit sRGB colour with straight (non-premultiplied) alpha.

#include <cstdint>
#include <string>
#include <string_view>

#include "roados/Result.h"

namespace roados {

namespace detail {
constexpr int hexDigit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/// Parses "#RGB", "#RGBA", "#RRGGBB" or "#RRGGBBAA". Returns false on malformed input.
constexpr bool parseHexColor(std::string_view s, std::uint8_t out[4]) noexcept {
    if (s.empty() || s[0] != '#') return false;
    s.remove_prefix(1);
    const auto n = s.size();
    if (n != 3 && n != 4 && n != 6 && n != 8) return false;
    for (char c : s) if (hexDigit(c) < 0) return false;
    out[3] = 255;
    if (n <= 4) {
        for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(hexDigit(s[i]) * 17);
    } else {
        for (std::size_t i = 0; i < n / 2; ++i)
            out[i] = static_cast<std::uint8_t>(hexDigit(s[2 * i]) * 16 + hexDigit(s[2 * i + 1]));
    }
    return true;
}
// Deliberately not constexpr: reaching it during constant evaluation is a compile error.
inline void invalid_color_literal() {}
}  // namespace detail

struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    /// Compile-time validated literal: Color("#2196F3"). An invalid literal does not compile.
    /// For runtime strings use Color::parse().
    consteval Color(const char* hex) {
        std::uint8_t c[4] = {0, 0, 0, 255};
        if (!detail::parseHexColor(std::string_view(hex), c)) detail::invalid_color_literal();
        r = c[0]; g = c[1]; b = c[2]; a = c[3];
    }

    /// Parses "#RGB", "#RGBA", "#RRGGBB", "#RRGGBBAA" or "transparent".
    static Result<Color> parse(std::string_view text);

    static constexpr Color rgb(std::uint32_t rrggbb) {
        return {std::uint8_t(rrggbb >> 16), std::uint8_t(rrggbb >> 8), std::uint8_t(rrggbb), 255};
    }
    static constexpr Color rgba(std::uint32_t rrggbbaa) {
        return {std::uint8_t(rrggbbaa >> 24), std::uint8_t(rrggbbaa >> 16), std::uint8_t(rrggbbaa >> 8),
                std::uint8_t(rrggbbaa)};
    }
    static constexpr Color Transparent() { return {0, 0, 0, 0}; }
    static constexpr Color Black() { return {0, 0, 0, 255}; }
    static constexpr Color White() { return {255, 255, 255, 255}; }

    constexpr Color withAlpha(std::uint8_t alpha) const { return {r, g, b, alpha}; }
    /// Multiplies alpha by `opacity` (clamped to 0..1).
    Color withOpacity(float opacity) const;

    /// "#RRGGBB" when opaque, otherwise "#RRGGBBAA" (upper-case).
    std::string toHex() const;
    constexpr std::uint32_t argb() const {
        return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | b;
    }
    constexpr bool isOpaque() const { return a == 255; }

    friend constexpr bool operator==(const Color&, const Color&) = default;
};

}  // namespace roados
