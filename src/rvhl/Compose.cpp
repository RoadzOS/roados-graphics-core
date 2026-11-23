// Port of the Visual Hazard Studio's composeIcon() (src/icons/compose.ts,
// Studio commit pinned in assets/rvhl/manifest.json). Every string template
// below mirrors the TypeScript source. Number formatting uses JavaScript
// semantics (rvhl/JsFormat.h) so the output is byte-identical; see
// tests/integration/ComposeParityTest.cpp.

#include <algorithm>
#include <cmath>
#include <numbers>

#include "roados/rvhl/Hazard.h"
#include "rvhl/JsFormat.h"

namespace roados::rvhl {

using detail::escapeXml;
using detail::jsNumber;
using detail::jsRound;
using detail::jsToFixed;

const std::vector<Layer>& defaultLayerOrder() {
    static const std::vector<Layer> order = {
        Layer::Background, Layer::Frame,     Layer::Pictogram, Layer::Slash, Layer::SeverityCode, Layer::SeverityBar,
        Layer::Lane,       Layer::Direction, Layer::Count,     Layer::SizeChip, Layer::Speed,     Layer::Distance,
    };
    return order;
}

const char* toString(Layer layer) noexcept {
    switch (layer) {
        case Layer::Background: return "background";
        case Layer::Frame: return "frame";
        case Layer::Pictogram: return "pictogram";
        case Layer::Slash: return "slash";
        case Layer::SeverityCode: return "severity_code";
        case Layer::SeverityBar: return "severity_bar";
        case Layer::Lane: return "lane";
        case Layer::Direction: return "direction";
        case Layer::Count: return "count";
        case Layer::SizeChip: return "size_chip";
        case Layer::Speed: return "speed";
        case Layer::Distance: return "distance";
    }
    return "unknown";
}

HazardOptions& HazardOptions::disable(Layer layer) {
    layers.erase(std::remove(layers.begin(), layers.end(), layer), layers.end());
    return *this;
}

HazardOptions& HazardOptions::enable(Layer layer) {
    if (isEnabled(layer)) return *this;
    // Insert before the first enabled layer that comes after it in the default order.
    const auto& def = defaultLayerOrder();
    const auto pos = std::find(def.begin(), def.end(), layer);
    auto insertAt = layers.end();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        if (std::find(def.begin(), def.end(), *it) > pos) {
            insertAt = it;
            break;
        }
    }
    layers.insert(insertAt, layer);
    return *this;
}

bool HazardOptions::isEnabled(Layer layer) const {
    return std::find(layers.begin(), layers.end(), layer) != layers.end();
}

Frame resolveFrame(const Entry& entry, Representation representation, Representation* resolved) {
    auto set = [&](Frame f, Representation r) {
        if (resolved) *resolved = r;
        return f;
    };
    if (representation == Representation::Standard && entry.standardFrame)
        return set(*entry.standardFrame, Representation::Standard);
    if (representation == Representation::RoadOS) {
        if (!isConventionFrame(entry.frame)) return set(entry.frame, Representation::RoadOS);
        return set(Frame::RoadOSHazard, Representation::RoadOS);
    }
    return set(entry.frame, isConventionFrame(entry.frame) ? Representation::Standard : Representation::RoadOS);
}

std::string formatDistance(double m, UnitSystem system) {
    if (system == UnitSystem::Imperial) {
        const double ft = m * 3.28084;
        if (ft < 1000) return jsNumber(jsRound(ft / 10) * 10) + " ft";
        const double mi = m / 1609.344;
        return (mi < 10 ? jsToFixed(mi, 1) : jsNumber(jsRound(mi))) + " mi";
    }
    if (m < 1000) return jsNumber(jsRound(m / 10) * 10) + " m";
    const double km = m / 1000;
    return (km < 10 ? jsToFixed(km, 1) : jsNumber(jsRound(km))) + " km";
}

namespace {

struct GlyphBox {
    double cx, cy, box;
};

GlyphBox glyphBox(Frame f) {
    switch (f) {
        case Frame::WarningTriangle: return {64, 67, 44};
        case Frame::GiveWayTriangle: return {64, 38, 34};
        case Frame::ProhibitoryCircle: return {64, 54, 58};
        case Frame::MandatoryCircle: return {64, 54, 60};
        case Frame::StopOctagon: return {64, 54, 80};
        case Frame::InfoRect: return {64, 54, 70};
        case Frame::DirectionBoard: return {64, 54, 76};
        case Frame::RoadOSHazard: return {64, 55, 66};
        case Frame::RoadOSMarking: return {64, 54, 76};
        case Frame::RoadOSInfrastructure: return {64, 58, 62};
        case Frame::RoadOSSignal: return {64, 54, 82};
        case Frame::RoadOSContext: return {64, 54, 58};
    }
    return {64, 55, 66};
}

std::string octagon(double cx, double cy, double r) {
    std::string d = "M";
    for (int i = 0; i < 8; ++i) {
        const double a = ((22.5 + 45 * i) * std::numbers::pi) / 180;
        if (i) d += "L";
        d += jsToFixed(cx + r * std::cos(a), 1) + " " + jsToFixed(cy + r * std::sin(a), 1);
    }
    return d + "Z";
}

struct FrameParts {
    std::string back, front;
};

class Composer {
public:
    Composer(const Tokens& t) : T(t), FONT(t.typography.family), NUM_FONT(t.typography.numeralFamily) {}

    std::string txt(double x, double y, double size, std::string_view s, const std::string& color, int weight = 800,
                     const std::string* font = nullptr) const {
        return "<text x=\"" + jsNumber(x) + "\" y=\"" + jsNumber(y) + "\" font-family=\"" + (font ? *font : FONT) +
               "\" font-size=\"" + jsNumber(size) + "\" font-weight=\"" + std::to_string(weight) + "\" fill=\"" +
               color + "\" text-anchor=\"middle\" dominant-baseline=\"central\">" + escapeXml(s) + "</text>";
    }

    FrameParts frameSvg(Frame frame, Theme theme, Severity sev, bool showCode) const {
        const auto& th = T.of(theme);
        const std::string& sc = T.of(sev).color;
        const std::string& so = T.of(sev).onColor;
        const double sw = T.stroke.frame;
        const auto& CV = T.convention;
        const std::string codeTab =
            showCode ? "<rect x=\"50\" y=\"0.5\" width=\"28\" height=\"15\" rx=\"7.5\" fill=\"" + sc + "\"/>" +
                           "<text x=\"64\" y=\"8.3\" font-family=\"" + FONT +
                           "\" font-size=\"10\" font-weight=\"800\" fill=\"" + so +
                           "\" text-anchor=\"middle\" dominant-baseline=\"central\">" + toString(sev) + "</text>"
                     : "";
        switch (frame) {
            case Frame::WarningTriangle:
                return {"<path d=\"M64 10L111 94H17Z\" fill=\"" + CV.signWhite + "\" stroke=\"" + CV.signRed +
                            "\" stroke-width=\"" + jsNumber(T.stroke.signBorder) + "\" stroke-linejoin=\"round\"/>",
                        ""};
            case Frame::GiveWayTriangle:
                return {"<path d=\"M17 14H111L64 98Z\" fill=\"" + CV.signWhite + "\" stroke=\"" + CV.signRed +
                            "\" stroke-width=\"" + jsNumber(T.stroke.signBorder + 2) + "\" stroke-linejoin=\"round\"/>",
                        ""};
            case Frame::ProhibitoryCircle:
                return {"<circle cx=\"64\" cy=\"54\" r=\"43\" fill=\"" + CV.signWhite + "\" stroke=\"" + CV.signRed +
                            "\" stroke-width=\"" + jsNumber(T.stroke.signBorder + 1) + "\"/>",
                        ""};
            case Frame::MandatoryCircle:
                return {"<circle cx=\"64\" cy=\"54\" r=\"46\" fill=\"" + CV.signBlue +
                            "\" stroke=\"#FFFFFF\" stroke-width=\"2.5\"/>",
                        ""};
            case Frame::StopOctagon:
                return {"<path d=\"" + octagon(64, 54, 48) + "\" fill=\"" + CV.signRed + "\"/><path d=\"" +
                            octagon(64, 54, 44) + "\" fill=\"none\" stroke=\"#FFFFFF\" stroke-width=\"3\"/>",
                        ""};
            case Frame::InfoRect:
                return {"<rect x=\"18\" y=\"8\" width=\"92\" height=\"92\" rx=\"8\" fill=\"" + CV.signBlue +
                            "\" stroke=\"#FFFFFF\" stroke-width=\"2.5\"/>",
                        ""};
            case Frame::DirectionBoard:
                return {"<rect x=\"12\" y=\"16\" width=\"104\" height=\"76\" rx=\"6\" fill=\"" + CV.signGreen +
                            "\"/><rect x=\"16\" y=\"20\" width=\"96\" height=\"68\" rx=\"4\" fill=\"none\" "
                            "stroke=\"#FFFFFF\" stroke-width=\"2.5\"/>",
                        ""};
            case Frame::RoadOSMarking:
                return {"<rect x=\"18\" y=\"8\" width=\"92\" height=\"92\" rx=\"" + jsNumber(T.radius.tile) +
                            "\" fill=\"" + th.markingFill + "\" stroke=\"" + sc + "\" stroke-width=\"" +
                            jsNumber(sw - 1) + "\"/>",
                        codeTab};
            case Frame::RoadOSInfrastructure:
                return {"<path d=\"M18 100V46Q18 8 64 8T110 46V100Z\" fill=\"" + th.infraFill + "\" stroke=\"" + sc +
                            "\" stroke-width=\"" + jsNumber(sw - 1) + "\" stroke-linejoin=\"round\"/>",
                        codeTab};
            case Frame::RoadOSSignal:
                return {"<rect x=\"24\" y=\"6\" width=\"80\" height=\"96\" rx=\"" + jsNumber(T.radius.signal) +
                            "\" fill=\"" + th.frameFill + "\" stroke=\"" + sc + "\" stroke-width=\"" +
                            jsNumber(sw - 1) + "\"/>",
                        codeTab};
            case Frame::RoadOSContext:
                return {"<path d=\"M112 54L88 95.6H40L16 54L40 12.4H88Z\" fill=\"" + th.contextFill +
                            "\" stroke=\"" + th.accentBlue + "\" stroke-width=\"4\" stroke-linejoin=\"round\"/>",
                        ""};
            case Frame::RoadOSHazard:
                break;
        }
        return {"<rect x=\"18\" y=\"8\" width=\"92\" height=\"92\" rx=\"" + jsNumber(T.radius.hazard) + "\" fill=\"" +
                    th.frameFill + "\" stroke=\"" + sc + "\" stroke-width=\"" + jsNumber(sw) + "\"/>",
                codeTab};
    }

    static double displaySpeed(double valueKmh, SpeedUnit unit) {
        return jsRound(unit == SpeedUnit::Mph ? valueKmh / 1.609344 : valueKmh);
    }

    std::string speedBadge(const SpeedBadge& b, double cx, double cy, double scale = 1) const {
        const auto& B = T.badges;
        const bool mph = b.unit == SpeedUnit::Mph;
        std::string s;
        if (b.kind == SpeedBadge::Kind::Posted && b.valueKmh) {
            const std::string v = jsNumber(displaySpeed(*b.valueKmh, b.unit));
            s = "<circle cx=\"0\" cy=\"0\" r=\"" + jsNumber(B.postedSpeed.radius) + "\" fill=\"" + B.postedSpeed.fill +
                "\" stroke=\"" + B.postedSpeed.ring + "\" stroke-width=\"" + jsNumber(B.postedSpeed.ringWidth) + "\"/>" +
                txt(0, mph ? -2 : 0.5, v.size() > 2 ? 13 : 16, v, B.postedSpeed.text, 900, &NUM_FONT) +
                (mph ? txt(0, 10, 6, "mph", B.postedSpeed.text, 700) : "");
        } else if (b.kind == SpeedBadge::Kind::Advisory && b.valueKmh) {
            const std::string v = jsNumber(displaySpeed(*b.valueKmh, b.unit));
            const auto& A = B.advisorySpeed;
            s = "<rect x=\"" + jsNumber(-A.width / 2) + "\" y=\"" + jsNumber(-A.height / 2) + "\" width=\"" +
                jsNumber(A.width) + "\" height=\"" + jsNumber(A.height) + "\" rx=\"" + jsNumber(A.radius) +
                "\" fill=\"" + A.fill + "\" stroke=\"" + A.stroke + "\" stroke-width=\"" + jsNumber(A.strokeWidth) +
                "\"/>" + txt(0, -8, 6.5, A.label + (mph ? " MPH" : " KM/H"), A.text, 800) +
                txt(0, 4, 14, v, A.text, 900, &NUM_FONT);
        } else {
            const auto& U = B.unknownSpeed;
            s = "<circle cx=\"0\" cy=\"0\" r=\"" + jsNumber(B.postedSpeed.radius) + "\" fill=\"" + U.fill +
                "\" stroke=\"" + U.ring + "\" stroke-width=\"3\" stroke-dasharray=\"4 3\"/>" +
                txt(0, 0.5, 16, "?", U.text, 900);
        }
        return "<g transform=\"translate(" + jsNumber(cx) + " " + jsNumber(cy) + ") scale(" + jsNumber(scale) + ")\">" +
               s + "</g>";
    }

    std::string directionChip(Direction dir) const {
        const auto& C = T.badges.chip;
        const char* a = "";
        switch (dir) {
            case Direction::Ahead:
                a = "<path d=\"M0 7V-5\" stroke=\"#F2F5F7\" stroke-width=\"3\" stroke-linecap=\"round\"/>"
                    "<path d=\"M-5 -2L0 -8L5 -2Z\" fill=\"#F2F5F7\"/>";
                break;
            case Direction::Left:
                a = "<path d=\"M7 0H-5\" stroke=\"#F2F5F7\" stroke-width=\"3\" stroke-linecap=\"round\"/>"
                    "<path d=\"M-2 -5L-8 0L-2 5Z\" fill=\"#F2F5F7\"/>";
                break;
            case Direction::Right:
                a = "<path d=\"M-7 0H5\" stroke=\"#F2F5F7\" stroke-width=\"3\" stroke-linecap=\"round\"/>"
                    "<path d=\"M2 -5L8 0L2 5Z\" fill=\"#F2F5F7\"/>";
                break;
            case Direction::Both:
                a = "<path d=\"M-5 0H5\" stroke=\"#F2F5F7\" stroke-width=\"3\"/>"
                    "<path d=\"M-3 -5L-9 0L-3 5ZM3 -5L9 0L3 5Z\" fill=\"#F2F5F7\"/>";
                break;
        }
        return "<g transform=\"translate(15 17)\"><circle r=\"13\" fill=\"" + C.fill + "\" stroke=\"" + C.stroke +
               "\" stroke-width=\"2\"/>" + a + "</g>";
    }

    std::string laneChip(int count, int index, Severity sev) const {
        const auto& C = T.badges.chip;
        const int n = std::max(1, std::min(6, count));
        const int w = 8 + n * 7;
        std::string bars;
        for (int i = 0; i < n; ++i)
            bars += "<rect x=\"" + std::to_string(4 + i * 7) + "\" y=\"5\" width=\"5\" height=\"14\" rx=\"1.5\" fill=\"" +
                    (i == index ? T.of(sev).color : std::string("#3A4A5A")) + "\"/>";
        return "<g transform=\"translate(2 4)\"><rect width=\"" + std::to_string(w) + "\" height=\"24\" rx=\"6\" fill=\"" +
               C.fill + "\" stroke=\"" + C.stroke + "\" stroke-width=\"2\"/>" + bars + "</g>";
    }

    const Tokens& T;
    const std::string FONT, NUM_FONT;
};

/// Body of a pictogram file: everything between the root <svg …> and </svg>.
std::string_view pictogramBody(std::string_view svg) {
    const auto open = svg.find("<svg");
    const auto start = open == std::string_view::npos ? open : svg.find('>', open);
    const auto end = svg.rfind("</svg>");
    if (start == std::string_view::npos || end == std::string_view::npos || end < start) return {};
    return svg.substr(start + 1, end - start - 1);
}

const char* sizeName(SizeVariant s) {
    switch (s) {
        case SizeVariant::S: return "S";
        case SizeVariant::M: return "M";
        case SizeVariant::L: return "L";
        case SizeVariant::XL: return "XL";
    }
    return "M";
}

}  // namespace

Result<ComposedHazard> compose(const Library& library, const Entry& entry, const HazardOptions& o) {
    if (library.empty()) return makeError(ErrorCode::InvalidArgument, "library is not loaded");
    const Tokens& T = library.tokens();
    const Composer c(T);

    const Theme theme = o.theme;
    const Severity sev = o.severity.value_or(entry.severity.defaultLevel);
    Representation rep = Representation::RoadOS;
    const Frame frame = resolveFrame(entry, o.representation, &rep);
    const bool standard = isConventionFrame(frame);

    ComposedHazard out;
    out.frame = frame;
    out.representation = rep;
    out.severity = sev;

    std::string parts;
    auto emit = [&](Layer layer, const std::string& markup) {
        if (markup.empty()) return;
        parts += markup;
        out.drawn.push_back(layer);
    };

    for (Layer layer : o.layers) {
        switch (layer) {
            case Layer::Background:
                if (o.background) emit(layer, "<rect width=\"128\" height=\"128\" fill=\"" + o.background->toHex() + "\"/>");
                break;
            case Layer::Frame:
                emit(layer, c.frameSvg(frame, theme, sev, o.showSeverityCode).back);
                break;
            case Layer::Pictogram: {
                auto svg = library.pictogramSvg(entry, frame, theme, o.variant);
                if (!svg) return svg.error();
                const GlyphBox g = glyphBox(frame);
                const double k = g.box / 64;
                emit(layer, "<g transform=\"translate(" + jsToFixed(g.cx - g.box / 2, 2) + " " +
                                jsToFixed(g.cy - g.box / 2, 2) + ") scale(" + jsToFixed(k, 4) + ")\">" +
                                std::string(pictogramBody(*svg)) + "</g>");
                break;
            }
            case Layer::Slash:
                if (entry.visual.slash)
                    emit(layer, frame == Frame::ProhibitoryCircle
                                    ? "<path d=\"M33.6 23.6L94.4 84.4\" stroke=\"" + T.convention.signRed +
                                          "\" stroke-width=\"9\" stroke-linecap=\"butt\"/>"
                                    : "<path d=\"M30 22L98 90\" stroke=\"" + T.convention.signRed +
                                          "\" stroke-width=\"8\" stroke-linecap=\"round\"/>");
                break;
            case Layer::SeverityCode:
                emit(layer, c.frameSvg(frame, theme, sev, o.showSeverityCode).front);
                break;
            case Layer::SeverityBar:
                if (standard)
                    emit(layer, "<rect x=\"36\" y=\"103\" width=\"56\" height=\"5\" rx=\"2.5\" fill=\"" + T.of(sev).color +
                                    "\"/>");
                break;
            case Layer::Lane:
                if (o.lane) emit(layer, c.laneChip(o.lane->count, o.lane->index, sev));
                break;
            case Layer::Direction:
                // The Studio draws the lane chip instead of the direction chip when both are set.
                if (o.direction && !o.lane) emit(layer, c.directionChip(*o.direction));
                break;
            case Layer::Count:
                if (o.count && *o.count > 1) {
                    const auto& C = T.badges.count;
                    emit(layer, "<g transform=\"translate(113 17)\"><circle r=\"" + jsNumber(C.radius) + "\" fill=\"" +
                                    C.fill + "\" stroke=\"#0B1118\" stroke-width=\"2\"/>" +
                                    c.txt(0, 0.5, 11, "\xC3\x97" + std::to_string(*o.count), C.text, 900) + "</g>");
                }
                break;
            case Layer::SizeChip:
                if (o.sizeVariant) {
                    const auto& C = T.badges.chip;
                    emit(layer, "<g transform=\"translate(2 78)\"><rect width=\"28\" height=\"20\" rx=\"6\" fill=\"" +
                                    C.fill + "\" stroke=\"" + C.stroke + "\" stroke-width=\"2\"/>" +
                                    c.txt(14, 10.5, 10, sizeName(*o.sizeVariant), C.text, 900) + "</g>");
                }
                break;
            case Layer::Speed:
                if (o.speed) emit(layer, c.speedBadge(*o.speed, 104, 82));
                break;
            case Layer::Distance:
                if (o.distanceM && *o.distanceM >= 0) {
                    const std::string label = formatDistance(*o.distanceM, o.unitSystem);
                    const auto& D = T.badges.distance;
                    const double w = 14 + static_cast<double>(label.size()) * 7;
                    emit(layer, "<rect x=\"" + jsNumber(64 - w / 2) + "\" y=\"109\" width=\"" + jsNumber(w) +
                                    "\" height=\"" + jsNumber(D.height) + "\" rx=\"" + jsNumber(D.radius) + "\" fill=\"" +
                                    D.fill + "\" stroke=\"" + D.stroke + "\" stroke-width=\"1.5\"/>" +
                                    c.txt(64, 118.5, T.typography.distanceSize, label, D.text, 800));
                }
                break;
        }
    }

    const std::string title = escapeXml(entry.id + " " + entry.name);
    out.svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 128 128\" width=\"128\" height=\"128\" "
              "role=\"img\" aria-label=\"" + title + "\"><title>" + title + "</title>" + parts + "</svg>";
    return out;
}

}  // namespace roados::rvhl
