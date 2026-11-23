// Port of the Studio's src/engine/severity.ts. Reason texts use the same
// wording and JavaScript number formatting; see SeverityParityTest.

#include "roados/rvhl/Severity.h"

#include <algorithm>
#include <unordered_set>

#include "rvhl/JsFormat.h"

namespace roados::rvhl {

using detail::jsNumber;
using detail::jsRound;
using C = SeverityContext;

namespace {
const char* name(C::PhysicalSize v) {
    switch (v) { case C::PhysicalSize::Small: return "small"; case C::PhysicalSize::Medium: return "medium"; case C::PhysicalSize::Large: return "large"; }
    return "";
}
const char* name(C::HeightOrDepth v) {
    switch (v) { case C::HeightOrDepth::Low: return "low"; case C::HeightOrDepth::Moderate: return "moderate"; case C::HeightOrDepth::High: return "high"; }
    return "";
}
// Lane position and vehicle type are printed with their first '_' replaced, as in the Studio.
const char* name(C::LanePosition v) {
    switch (v) { case C::LanePosition::InPath: return "in path"; case C::LanePosition::AdjacentLane: return "adjacent lane"; case C::LanePosition::Shoulder: return "shoulder"; case C::LanePosition::OffRoad: return "off road"; }
    return "";
}
const char* name(C::Visibility v) {
    switch (v) { case C::Visibility::Good: return "good"; case C::Visibility::Reduced: return "reduced"; case C::Visibility::Poor: return "poor"; }
    return "";
}
const char* name(C::Surface v) {
    switch (v) { case C::Surface::Dry: return "dry"; case C::Surface::Wet: return "wet"; case C::Surface::Flooded: return "flooded"; case C::Surface::Loose: return "loose"; }
    return "";
}
const char* name(C::TrafficDensity v) {
    switch (v) { case C::TrafficDensity::Free: return "free"; case C::TrafficDensity::Moderate: return "moderate"; case C::TrafficDensity::Heavy: return "heavy"; }
    return "";
}
const char* name(C::VehicleType v) {
    switch (v) { case C::VehicleType::TwoWheeler: return "two-wheeler"; case C::VehicleType::Car: return "car"; case C::VehicleType::Bus: return "bus"; case C::VehicleType::Truck: return "truck"; }
    return "";
}
}  // namespace

double stoppingDistanceM(double speedKmh, C::Surface surface) {
    const double v = speedKmh / 3.6;
    const double mu = surface == C::Surface::Dry ? 0.7 : surface == C::Surface::Wet ? 0.45 : surface == C::Surface::Loose ? 0.35 : 0.25;
    return v * 1.5 + (v * v) / (2 * mu * 9.81);
}

SeverityResult computeSeverity(const Entry& entry, const SeverityContext& c) {
    SeverityResult r;
    r.base = entry.severity.defaultLevel;
    const std::unordered_set<std::string> f(entry.severity.factors.begin(), entry.severity.factors.end());
    auto add = [&](const char* factor, int effect, std::string text) {
        if (f.count(factor) && effect != 0) r.reasons.push_back({factor, effect, std::move(text)});
    };

    if (entry.severity.dynamic) {
        add("physical_size", c.physicalSize == C::PhysicalSize::Large ? 1 : c.physicalSize == C::PhysicalSize::Small ? -1 : 0,
            std::string("Physical size: ") + name(c.physicalSize));
        add("height_or_depth", c.heightOrDepth == C::HeightOrDepth::High ? 1 : c.heightOrDepth == C::HeightOrDepth::Low ? -1 : 0,
            std::string("Height/depth: ") + name(c.heightOrDepth));
        add("lane_position",
            c.lanePosition == C::LanePosition::InPath ? 0 : c.lanePosition == C::LanePosition::AdjacentLane ? -1 : -2,
            std::string("Lane position: ") + name(c.lanePosition));
        add("approach_speed",
            c.approachSpeedKmh >= 90 ? 2 : c.approachSpeedKmh >= 60 ? 1 : c.approachSpeedKmh <= 25 ? -1 : 0,
            "Approach speed " + jsNumber(c.approachSpeedKmh) + " km/h");
        const double sd = stoppingDistanceM(c.approachSpeedKmh, c.surface);
        add("distance", c.distanceM < sd ? 1 : 0,
            "Distance " + jsNumber(c.distanceM) + " m vs stopping distance \xE2\x89\x88 " + jsNumber(jsRound(sd)) + " m");
        add("visibility", c.visibility == C::Visibility::Poor ? 1 : 0, std::string("Visibility: ") + name(c.visibility));
        add("surface_condition", c.surface == C::Surface::Dry ? 0 : 1, std::string("Surface: ") + name(c.surface));
        add("traffic_density", c.trafficDensity == C::TrafficDensity::Heavy ? 1 : 0,
            std::string("Traffic: ") + name(c.trafficDensity));
        add("vehicle_type", c.vehicleType == C::VehicleType::TwoWheeler ? 1 : 0,
            std::string("Vehicle: ") + name(c.vehicleType));
    }
    int sum = 0;
    for (const auto& x : r.reasons) sum += x.effect;
    r.delta = std::max(-2, std::min(2, sum));
    const int idx = std::max(0, std::min(4, static_cast<int>(r.base) - 1 + r.delta));
    r.level = static_cast<Severity>(idx + 1);
    r.display = c.confidence >= entry.confidence.minimumToDisplay;
    r.alert = c.confidence >= entry.confidence.minimumToAlert;
    return r;
}

std::optional<Advisory> computeAdvisory(const Entry& entry, Severity level, std::optional<double> postedKmh,
                                        double confidence) {
    if (entry.speed.advisory == "never") return std::nullopt;
    double v = 0;
    switch (level) {
        case Severity::S1: return std::nullopt;
        case Severity::S2: v = 40; break;
        case Severity::S3: v = 30; break;
        case Severity::S4: v = 20; break;
        case Severity::S5: v = 10; break;
    }
    const double capped = postedKmh ? std::min(v, *postedKmh) : v;
    if (postedKmh && capped >= *postedKmh) return std::nullopt;
    return Advisory{capped, jsRound(confidence * 0.9 * 100) / 100};
}

}  // namespace roados::rvhl
