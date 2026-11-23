#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>

#include "include/core/SkStream.h"
#include "modules/skshaper/utils/FactoryHelpers.h"
#include "modules/svg/include/SkSVGSVG.h"
#include "renderer/RendererImpl.h"
#include "svg/SvgImpl.h"

namespace roados {

namespace {

struct Attr {
    std::string name;
    std::string value;  // XML-unescaped
};

std::string xmlUnescape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            static constexpr std::pair<std::string_view, char> kEnt[] = {
                {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            bool matched = false;
            for (auto [ent, ch] : kEnt)
                if (s.substr(i, ent.size()) == ent) { out += ch; i += ent.size() - 1; matched = true; break; }
            if (matched) continue;
        }
        out += s[i];
    }
    return out;
}

std::string xmlEscapeAttr(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

/// Parses `name="v"` / `name='v'` pairs from the inside of a start tag.
bool parseAttributes(std::string_view s, std::vector<Attr>& out) {
    std::size_t i = 0;
    auto skipWs = [&] { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; };
    while (true) {
        skipWs();
        if (i >= s.size()) return true;
        const std::size_t n0 = i;
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i])) && s[i] != '=') ++i;
        std::string name(s.substr(n0, i - n0));
        skipWs();
        if (i >= s.size() || s[i] != '=') return false;
        ++i;
        skipWs();
        if (i >= s.size() || (s[i] != '"' && s[i] != '\'')) return false;
        const char q = s[i++];
        const std::size_t v0 = i;
        while (i < s.size() && s[i] != q) ++i;
        if (i >= s.size()) return false;
        out.push_back({std::move(name), xmlUnescape(s.substr(v0, i - v0))});
        ++i;
    }
}

Attr* find(std::vector<Attr>& attrs, std::string_view name) {
    for (auto& a : attrs) if (a.name == name) return &a;
    return nullptr;
}

// Locale-independent decimal parser (libc++ lacks floating-point from_chars,
// and strtof depends on the process locale's decimal separator).
std::optional<float> parseNumber(std::string_view v) {
    std::size_t i = 0;
    double sign = 1, value = 0;
    if (i < v.size() && (v[i] == '+' || v[i] == '-')) sign = v[i++] == '-' ? -1 : 1;
    bool digits = false;
    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) { value = value * 10 + (v[i++] - '0'); digits = true; }
    if (i < v.size() && v[i] == '.') {
        ++i;
        double scale = 0.1;
        while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) { value += (v[i++] - '0') * scale; scale /= 10; digits = true; }
    }
    if (!digits) return std::nullopt;
    if (i < v.size() && (v[i] == 'e' || v[i] == 'E')) {
        ++i;
        int esign = 1, exp = 0;
        if (i < v.size() && (v[i] == '+' || v[i] == '-')) esign = v[i++] == '-' ? -1 : 1;
        if (i >= v.size() || !std::isdigit(static_cast<unsigned char>(v[i]))) return std::nullopt;
        while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) exp = exp * 10 + (v[i++] - '0');
        value *= std::pow(10.0, esign * exp);
    }
    if (i != v.size()) return std::nullopt;
    return float(sign * value);
}

std::optional<float> parseLength(std::string_view v) {
    while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
    if (v.size() > 2 && v.substr(v.size() - 2) == "px") v.remove_suffix(2);
    return parseNumber(v);
}

std::optional<int> parseWeight(std::string_view v) {
    if (v == "normal") return 400;
    if (v == "bold") return 700;
    int w = 0;
    auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), w);
    if (ec != std::errc() || p != v.data() + v.size()) return std::nullopt;
    return w;
}

std::string formatNumber(float v) {
    char buf[32];
    auto [end, ec] = std::to_chars(buf, buf + sizeof buf, std::round(v * 1000.0f) / 1000.0f);
    return ec == std::errc() ? std::string(buf, end) : "0";
}

/// Finds the end of a start tag beginning at `from` ('>' outside quotes).
std::size_t tagEnd(std::string_view s, std::size_t from) {
    char q = 0;
    for (std::size_t i = from; i < s.size(); ++i) {
        if (q) { if (s[i] == q) q = 0; }
        else if (s[i] == '"' || s[i] == '\'') q = s[i];
        else if (s[i] == '>') return i;
    }
    return std::string_view::npos;
}

}  // namespace

std::string normalizeSvgText(std::string_view svg, const Renderer& renderer, SvgReport& report) {
    std::string out;
    out.reserve(svg.size() + 64);
    std::size_t pos = 0;
    while (true) {
        const std::size_t lt = svg.find('<', pos);
        if (lt == std::string_view::npos) { out.append(svg.substr(pos)); break; }
        out.append(svg.substr(pos, lt - pos));
        const std::size_t gt = tagEnd(svg, lt);
        if (gt == std::string_view::npos) { out.append(svg.substr(lt)); break; }
        std::string_view tag = svg.substr(lt, gt - lt + 1);
        pos = gt + 1;

        // Element name.
        std::size_t ne = 1;
        while (ne < tag.size() && !std::isspace(static_cast<unsigned char>(tag[ne])) && tag[ne] != '>' && tag[ne] != '/') ++ne;
        const std::string_view elem = tag.substr(1, ne - 1);
        const bool selfClosing = tag.size() >= 2 && tag[tag.size() - 2] == '/';
        if (elem.empty() || elem[0] == '/' || elem[0] == '!' || elem[0] == '?') { out.append(tag); continue; }

        std::string_view inner = tag.substr(ne, tag.size() - ne - (selfClosing ? 2 : 1));
        std::vector<Attr> attrs;
        if (!parseAttributes(inner, attrs)) {
            report.warnings.push_back("could not parse attributes of <" + std::string(elem) + ">; left unchanged");
            out.append(tag);
            continue;
        }

        const bool isText = elem == "text";
        Attr* baseline = find(attrs, "dominant-baseline");
        Attr* family = find(attrs, "font-family");
        if (isText) ++report.textElements;
        if (!isText && baseline) {
            report.warnings.push_back("dominant-baseline on <" + std::string(elem) + "> is not supported; ignored by Skia");
        }
        if (!family && !(isText && baseline)) { out.append(tag); continue; }

        bool changed = false;
        Font font;
        if (family) font.families = Font::parseFamilyList(family->value);
        const TextMetrics metrics = renderer.measure(
            Text("x").font(Font{font.families, 1, 400, false}));  // family resolution only

        if (family && !metrics.resolvedFamily.empty() && family->value != metrics.resolvedFamily) {
            family->value = metrics.resolvedFamily;
            ++report.fontFamiliesResolved;
            changed = true;
        }

        if (isText && baseline) {
            if (baseline->value != "central") {
                report.warnings.push_back("dominant-baseline=\"" + baseline->value + "\" is not supported; ignored");
            } else {
                const Attr* sizeAttr = find(attrs, "font-size");
                const Attr* weightAttr = find(attrs, "font-weight");
                const auto size = sizeAttr ? parseLength(sizeAttr->value) : std::nullopt;
                const auto weight = weightAttr ? parseWeight(weightAttr->value) : std::optional<int>(400);
                Attr* yAttr = find(attrs, "y");
                std::vector<float> ys;
                bool ysValid = true;
                if (yAttr) {
                    // y may be a list of coordinates (one per character).
                    std::string_view v = yAttr->value;
                    while (!v.empty()) {
                        const std::size_t s = v.find_first_not_of(" ,\t\n");
                        if (s == std::string_view::npos) break;
                        v.remove_prefix(s);
                        const std::size_t e = std::min(v.find_first_of(" ,\t\n"), v.size());
                        const auto n = parseLength(v.substr(0, e));
                        if (!n) { ysValid = false; break; }
                        ys.push_back(*n);
                        v.remove_prefix(e);
                    }
                } else {
                    ys.push_back(0);  // SVG default
                }
                if (!family || !size || !weight) {
                    report.warnings.push_back(
                        "dominant-baseline=\"central\" on <text> without explicit font-family/font-size; not adjusted");
                } else if (!ysValid || ys.empty()) {
                    report.warnings.push_back("dominant-baseline=\"central\" with unparseable y; not adjusted");
                } else {
                    font.size = *size;
                    font.weight = *weight;
                    const TextMetrics m = renderer.measure(Text("x").font(font));
                    const float shift = (m.ascent - m.descent) / 2;
                    // Shift the absolute y, not dy: Skia m158 applies dy per glyph without
                    // accumulating it, which would move only the first character.
                    std::string shifted;
                    for (std::size_t k = 0; k < ys.size(); ++k) shifted += (k ? " " : "") + formatNumber(ys[k] + shift);
                    if (yAttr) yAttr->value = shifted;
                    else attrs.push_back({"y", shifted});
                    attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                                               [](const Attr& a) { return a.name == "dominant-baseline"; }),
                                attrs.end());
                    ++report.baselinesAdjusted;
                    changed = true;
                }
            }
        }

        if (!changed) { out.append(tag); continue; }
        out += '<';
        out.append(elem);
        for (const auto& a : attrs) {
            out += ' ';
            out += a.name;
            out += "=\"";
            out += xmlEscapeAttr(a.value);
            out += '"';
        }
        out += selfClosing ? "/>" : ">";
    }
    return out;
}

Result<SvgDocument> SvgDocument::parse(std::string_view svg, const SvgOptions& options, const Renderer& renderer) {
    auto impl = std::make_shared<Impl>();
    impl->source = options.normalizeText ? normalizeSvgText(svg, renderer, impl->report) : std::string(svg);

    SkMemoryStream stream(impl->source.data(), impl->source.size(), /*copyData=*/false);
    impl->dom = SkSVGDOM::Builder()
                    .setFontManager(renderer.impl().fontMgr)
                    .setTextShapingFactory(SkShapers::BestAvailable())
                    .make(stream);
    if (!impl->dom || !impl->dom->getRoot()) return makeError(ErrorCode::Parse, "SVG could not be parsed");

    const SkSVGSVG* root = impl->dom->getRoot();
    if (const auto& vb = root->getViewBox(); vb.has_value())
        impl->viewBox = Rect{vb->x(), vb->y(), vb->width(), vb->height()};
    SkSize s = impl->dom->containerSize();
    if ((s.isEmpty()) && impl->viewBox) s = SkSize::Make(impl->viewBox->width, impl->viewBox->height);
    if (s.isEmpty()) return makeError(ErrorCode::Unsupported, "SVG has neither width/height nor a viewBox");
    impl->dom->setContainerSize(s);
    impl->size = {s.width(), s.height()};
    return SvgDocument(std::move(impl));
}

Result<SvgDocument> SvgDocument::load(const std::filesystem::path& file, const SvgOptions& options,
                                      const Renderer& renderer) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return makeError(ErrorCode::Io, "cannot open " + file.string());
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto doc = parse(text, options, renderer);
    if (!doc) return makeError(doc.error().code, doc.error().message + " (" + file.string() + ")");
    return doc;
}

Size SvgDocument::intrinsicSize() const { return impl_ ? impl_->size : Size{}; }
std::optional<Rect> SvgDocument::viewBox() const { return impl_ ? impl_->viewBox : std::nullopt; }
const SvgReport& SvgDocument::report() const {
    static const SvgReport kEmpty;
    return impl_ ? impl_->report : kEmpty;
}
const std::string& SvgDocument::normalizedSource() const {
    static const std::string kEmpty;
    return impl_ ? impl_->source : kEmpty;
}

Rect SvgShape::box() const {
    const Size s = doc_.intrinsicSize();
    return {box_.x, box_.y, box_.width > 0 ? box_.width : s.width, box_.height > 0 ? box_.height : s.height};
}

Transform SvgShape::placement() const {
    const Size s = doc_.intrinsicSize();
    const Rect b = box();
    if (!(s.width > 0 && s.height > 0)) return Transform::Translate(b.x, b.y);
    float sx = b.width / s.width, sy = b.height / s.height;
    float tx = b.x, ty = b.y;
    if (fit_ == AspectFit::Meet) {
        const float k = std::min(sx, sy);
        tx += (b.width - s.width * k) / 2;
        ty += (b.height - s.height * k) / 2;
        sx = sy = k;
    }
    return Transform::Translate(tx, ty).scale(sx, sy);
}

}  // namespace roados
