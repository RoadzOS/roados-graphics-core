// computeSeverity()/computeAdvisory() must match the Studio's severity.ts.
// Golden vectors: tests/fixtures/rvhl-golden/severity.json.
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "roados/rvhl/Rvhl.h"

using namespace roados::rvhl;
using nlohmann::json;
namespace fs = std::filesystem;
using C = SeverityContext;

namespace {
SeverityContext toContext(const json& j) {
    SeverityContext c;
    const auto s = [&](const char* k) { return j[k].get<std::string>(); };
    c.physicalSize = s("physicalSize") == "small" ? C::PhysicalSize::Small : s("physicalSize") == "large" ? C::PhysicalSize::Large : C::PhysicalSize::Medium;
    c.heightOrDepth = s("heightOrDepth") == "low" ? C::HeightOrDepth::Low : s("heightOrDepth") == "high" ? C::HeightOrDepth::High : C::HeightOrDepth::Moderate;
    const auto lp = s("lanePosition");
    c.lanePosition = lp == "in_path" ? C::LanePosition::InPath : lp == "adjacent_lane" ? C::LanePosition::AdjacentLane : lp == "shoulder" ? C::LanePosition::Shoulder : C::LanePosition::OffRoad;
    c.approachSpeedKmh = j["approachSpeedKmh"].get<double>();
    c.distanceM = j["distanceM"].get<double>();
    c.visibility = s("visibility") == "good" ? C::Visibility::Good : s("visibility") == "reduced" ? C::Visibility::Reduced : C::Visibility::Poor;
    const auto sf = s("surface");
    c.surface = sf == "dry" ? C::Surface::Dry : sf == "wet" ? C::Surface::Wet : sf == "flooded" ? C::Surface::Flooded : C::Surface::Loose;
    c.trafficDensity = s("trafficDensity") == "free" ? C::TrafficDensity::Free : s("trafficDensity") == "heavy" ? C::TrafficDensity::Heavy : C::TrafficDensity::Moderate;
    const auto v = s("vehicleType");
    c.vehicleType = v == "two_wheeler" ? C::VehicleType::TwoWheeler : v == "car" ? C::VehicleType::Car : v == "bus" ? C::VehicleType::Bus : C::VehicleType::Truck;
    c.confidence = j["confidence"].get<double>();
    return c;
}

void expectAdvisory(const std::optional<Advisory>& got, const json& want, const std::string& where) {
    if (want.is_null()) {
        EXPECT_FALSE(got) << where;
        return;
    }
    ASSERT_TRUE(got) << where;
    EXPECT_EQ(got->valueKmh, want["valueKmh"].get<double>()) << where;
    EXPECT_EQ(got->confidence, want["confidence"].get<double>()) << where;
}
}  // namespace

TEST(SeverityParity, MatchesStudioSeverityEngine) {
    auto lib = Library::load(fs::path(ROADOS_ASSETS_DIR) / "rvhl");
    ASSERT_TRUE(lib) << lib.error().message;
    std::ifstream in(fs::path(ROADOS_FIXTURES_DIR) / "rvhl-golden" / "severity.json");
    const json golden = json::parse(in);

    std::vector<SeverityContext> contexts;
    for (const auto& c : golden["contexts"]) contexts.push_back(toContext(c));

    int checked = 0;
    for (const auto& c : golden["cases"]) {
        const Entry* e = lib->find(c["entry"].get<std::string>());
        ASSERT_NE(e, nullptr);
        const int ctx = c["context"].get<int>();
        const std::string where = e->id + " ctx " + std::to_string(ctx);
        const auto r = computeSeverity(*e, contexts[static_cast<std::size_t>(ctx)]);
        EXPECT_STREQ(toString(r.level), c["level"].get<std::string>().c_str()) << where;
        EXPECT_STREQ(toString(r.base), c["base"].get<std::string>().c_str()) << where;
        EXPECT_EQ(r.delta, c["delta"].get<int>()) << where;
        EXPECT_EQ(r.display, c["display"].get<bool>()) << where;
        EXPECT_EQ(r.alert, c["alert"].get<bool>()) << where;
        ASSERT_EQ(r.reasons.size(), c["reasons"].size()) << where;
        for (std::size_t i = 0; i < r.reasons.size(); ++i) {
            EXPECT_EQ(r.reasons[i].factor, c["reasons"][i]["factor"].get<std::string>()) << where;
            EXPECT_EQ(r.reasons[i].effect, c["reasons"][i]["effect"].get<int>()) << where;
            EXPECT_EQ(r.reasons[i].text, c["reasons"][i]["text"].get<std::string>()) << where;
        }
        const double conf = contexts[static_cast<std::size_t>(ctx)].confidence;
        expectAdvisory(computeAdvisory(*e, r.level, std::nullopt, conf), c["advisory"]["none"], where + " none");
        expectAdvisory(computeAdvisory(*e, r.level, 30.0, conf), c["advisory"]["posted30"], where + " posted30");
        expectAdvisory(computeAdvisory(*e, r.level, 50.0, conf), c["advisory"]["posted50"], where + " posted50");
        ++checked;
    }
    EXPECT_GT(checked, 3000);
}

TEST(Severity, StoppingDistance) {
    // 50 km/h dry: 13.89 m/s × 1.5 s + 13.89² / (2 × 0.7 × 9.81) ≈ 20.83 + 14.05
    EXPECT_NEAR(stoppingDistanceM(50), 34.88, 0.01);
    EXPECT_GT(stoppingDistanceM(50, C::Surface::Flooded), stoppingDistanceM(50, C::Surface::Wet));
}
