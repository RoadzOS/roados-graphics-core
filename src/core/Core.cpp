// Pure value-type implementations: Result, Color, Transform, Font, Group, Version.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "roados/Color.h"
#include "roados/Result.h"
#include "roados/Scene.h"
#include "roados/Text.h"
#include "roados/Transform.h"
#include "roados/Version.h"

namespace roados {

const char* toString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::Parse: return "Parse";
        case ErrorCode::Io: return "Io";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::Unsupported: return "Unsupported";
        case ErrorCode::Render: return "Render";
        case ErrorCode::Validation: return "Validation";
        case ErrorCode::Incompatible: return "Incompatible";
    }
    return "Unknown";
}

// ---- Color -----------------------------------------------------------------

Result<Color> Color::parse(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    if (text == "transparent") return Color::Transparent();
    std::uint8_t c[4] = {0, 0, 0, 255};
    if (!detail::parseHexColor(text, c))
        return makeError(ErrorCode::Parse, "invalid colour '" + std::string(text) +
                                               "' (expected #RGB, #RGBA, #RRGGBB, #RRGGBBAA or 'transparent')");
    return Color(c[0], c[1], c[2], c[3]);
}

Color Color::withOpacity(float opacity) const {
    const float o = std::clamp(opacity, 0.0f, 1.0f);
    return withAlpha(static_cast<std::uint8_t>(std::lround(a * o)));
}

std::string Color::toHex() const {
    char buf[10];
    if (a == 255) std::snprintf(buf, sizeof buf, "#%02X%02X%02X", r, g, b);
    else std::snprintf(buf, sizeof buf, "#%02X%02X%02X%02X", r, g, b, a);
    return buf;
}

// ---- Transform -------------------------------------------------------------

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
}

Transform Transform::Rotate(float degrees) {
    // Exact values for multiples of 90° avoid -0/epsilon noise in common cases.
    const float m = std::fmod(degrees, 360.0f);
    float s, c;
    if (m == 0) { s = 0; c = 1; }
    else if (m == 90 || m == -270) { s = 1; c = 0; }
    else if (m == 180 || m == -180) { s = 0; c = -1; }
    else if (m == 270 || m == -90) { s = -1; c = 0; }
    else { s = std::sin(degrees * kDegToRad); c = std::cos(degrees * kDegToRad); }
    return {c, s, -s, c, 0, 0};
}

Transform Transform::Rotate(float degrees, float cx, float cy) {
    return Translate(cx, cy) * Rotate(degrees) * Translate(-cx, -cy);
}

Transform Transform::SkewX(float degrees) { return {1, 0, std::tan(degrees * kDegToRad), 1, 0, 0}; }
Transform Transform::SkewY(float degrees) { return {1, std::tan(degrees * kDegToRad), 0, 1, 0, 0}; }

Rect Transform::mapRect(const Rect& r) const {
    const Point p[4] = {map({r.left(), r.top()}), map({r.right(), r.top()}), map({r.right(), r.bottom()}),
                        map({r.left(), r.bottom()})};
    float l = p[0].x, t = p[0].y, rr = p[0].x, b = p[0].y;
    for (const auto& q : p) {
        l = std::min(l, q.x); t = std::min(t, q.y);
        rr = std::max(rr, q.x); b = std::max(b, q.y);
    }
    return Rect::fromLTRB(l, t, rr, b);
}

std::optional<Transform> Transform::inverted() const {
    const double det = double(a_) * d_ - double(b_) * c_;
    if (!std::isfinite(det) || std::fabs(det) < 1e-12) return std::nullopt;
    const double id = 1.0 / det;
    return Transform(float(d_ * id), float(-b_ * id), float(-c_ * id), float(a_ * id),
                     float((double(c_) * f_ - double(d_) * e_) * id), float((double(b_) * e_ - double(a_) * f_) * id));
}

// ---- Font ------------------------------------------------------------------

std::vector<std::string> Font::parseFamilyList(std::string_view css) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i <= css.size()) {
        std::size_t j = css.find(',', i);
        if (j == std::string_view::npos) j = css.size();
        std::string_view item = css.substr(i, j - i);
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) item.remove_prefix(1);
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) item.remove_suffix(1);
        if (item.size() >= 2 && (item.front() == '\'' || item.front() == '"') && item.back() == item.front())
            item = item.substr(1, item.size() - 2);
        if (!item.empty()) out.emplace_back(item);
        i = j + 1;
    }
    return out;
}

// ---- Group -----------------------------------------------------------------

Group* Group::findGroup(std::string_view n) {
    for (auto& c : children_)
        if (auto* g = c.as<Group>(); g && g->name() == n) return g;
    return nullptr;
}

const Group* Group::findGroup(std::string_view n) const { return const_cast<Group*>(this)->findGroup(n); }

bool Group::move(std::size_t from, std::size_t to) {
    if (from >= children_.size() || to >= children_.size()) return false;
    if (from == to) return true;
    Node n = std::move(children_[from]);
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from));
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(to), std::move(n));
    return true;
}

bool Group::remove(std::size_t index) {
    if (index >= children_.size()) return false;
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

// ---- Version ---------------------------------------------------------------

Version libraryVersion() noexcept { return {ROADOS_VERSION_MAJOR, ROADOS_VERSION_MINOR, ROADOS_VERSION_PATCH}; }
const char* skiaRevision() noexcept { return ROADOS_SKIA_REVISION; }

}  // namespace roados
