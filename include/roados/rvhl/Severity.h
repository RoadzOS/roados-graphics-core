#pragma once
/// RoadOS dynamic severity and advisory speed (RVHL 1.0 proposal, not an
/// official classification). Ported from the Studio's src/engine/severity.ts
/// and verified against golden vectors produced by that code.
///
/// Severity describes potential consequence. Detection confidence is kept
/// separate: it only gates whether a hazard is displayed or alerted.

#include <optional>
#include <string>
#include <vector>

#include "roados/rvhl/Catalogue.h"

namespace roados::rvhl {

struct SeverityContext {
    enum class PhysicalSize { Small, Medium, Large };
    enum class HeightOrDepth { Low, Moderate, High };
    enum class LanePosition { InPath, AdjacentLane, Shoulder, OffRoad };
    enum class Visibility { Good, Reduced, Poor };
    enum class Surface { Dry, Wet, Flooded, Loose };
    enum class TrafficDensity { Free, Moderate, Heavy };
    enum class VehicleType { TwoWheeler, Car, Bus, Truck };

    PhysicalSize physicalSize = PhysicalSize::Medium;
    HeightOrDepth heightOrDepth = HeightOrDepth::Moderate;
    LanePosition lanePosition = LanePosition::InPath;
    double approachSpeedKmh = 50;
    double distanceM = 200;
    Visibility visibility = Visibility::Good;
    Surface surface = Surface::Dry;
    TrafficDensity trafficDensity = TrafficDensity::Moderate;
    VehicleType vehicleType = VehicleType::Car;
    /// Detection confidence 0–1. Does NOT change severity.
    double confidence = 0.85;
};

struct SeverityReason {
    std::string factor;  ///< e.g. "approach_speed"
    int effect = 0;      ///< −2 … +2
    std::string text;    ///< human-readable explanation (same wording as the Studio)
};

struct SeverityResult {
    Severity level = Severity::S3;
    Severity base = Severity::S3;
    int delta = 0;  ///< clamped to −2 … +2
    std::vector<SeverityReason> reasons;
    bool display = false;  ///< confidence ≥ entry.confidence.minimumToDisplay
    bool alert = false;    ///< confidence ≥ entry.confidence.minimumToAlert
};

struct Advisory {
    double valueKmh = 0;
    double confidence = 0;
};

/// Simple stopping distance: 1.5 s reaction + braking (μ 0.7 dry, 0.45 wet,
/// 0.35 loose, 0.25 flooded).
double stoppingDistanceM(double speedKmh, SeverityContext::Surface surface = SeverityContext::Surface::Dry);

SeverityResult computeSeverity(const Entry& entry, const SeverityContext& context);

/// Advisory speed for a severity level (S2→40, S3→30, S4→20, S5→10 km/h),
/// capped by the posted limit. Returns nothing when no advisory applies.
/// It is NEVER a legal limit.
std::optional<Advisory> computeAdvisory(const Entry& entry, Severity level, std::optional<double> postedKmh,
                                        double confidence);

}  // namespace roados::rvhl
