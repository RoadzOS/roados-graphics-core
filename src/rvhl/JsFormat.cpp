#include "rvhl/JsFormat.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace roados::rvhl::detail {

std::string jsNumber(double x) {
    if (std::isnan(x)) return "NaN";
    if (std::isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
    if (x == 0) return "0";  // also maps -0 to "0", as JavaScript does
    std::array<char, 64> buf{};
    // Shortest round-trip digits, the same rule ECMAScript Number::toString uses.
    // Plain (non-exponent) form covers every value composition produces.
    if (std::fabs(x) < 1e21 && std::fabs(x) >= 1e-6) {
        auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), x, std::chars_format::fixed);
        if (ec == std::errc()) return std::string(buf.data(), end);
    }
    auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), x);
    return ec == std::errc() ? std::string(buf.data(), end) : std::string("NaN");
}

std::string jsToFixed(double x, int digits) {
    if (std::isnan(x)) return "NaN";
    if (std::fabs(x) >= 1e21) return jsNumber(x);
    std::string sign;
    if (x < 0) {
        sign = "-";
        x = -x;
    }
    // Exact decimal expansion of the double (printf prints exact digits on the
    // platforms we support), then round half-up at `digits`. A double's
    // fractional part has at most 52 significant binary digits, so 60 decimal
    // places are exact for every value composition produces.
    char buf[512];
    std::snprintf(buf, sizeof buf, "%.60f", x);
    std::string s(buf);
    const auto dot = s.find('.');
    std::string intPart = s.substr(0, dot);
    std::string frac = s.substr(dot + 1);
    const bool roundUp = frac[static_cast<std::size_t>(digits)] >= '5';
    std::string kept = intPart + frac.substr(0, static_cast<std::size_t>(digits));
    if (roundUp) {
        int i = static_cast<int>(kept.size()) - 1;
        while (i >= 0) {
            if (kept[static_cast<std::size_t>(i)] == '9') {
                kept[static_cast<std::size_t>(i)] = '0';
                --i;
            } else {
                ++kept[static_cast<std::size_t>(i)];
                break;
            }
        }
        if (i < 0) kept.insert(kept.begin(), '1');
    }
    const std::size_t intLen = kept.size() - static_cast<std::size_t>(digits);
    std::string out = sign + kept.substr(0, intLen);
    if (digits > 0) out += "." + kept.substr(intLen);
    return out;
}

double jsRound(double x) {
    const double f = std::floor(x);
    return (x - f >= 0.5) ? f + 1 : f;
}

std::string escapeXml(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

}  // namespace roados::rvhl::detail
