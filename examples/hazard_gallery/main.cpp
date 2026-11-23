// Renders RVHL hazard galleries with RoadOS Graphics Core: one icon per
// catalogue family (dark and light), the S1–S5 severity scale, and badge
// compositions. Every pixel comes from the engine and the Studio's catalogue.
//
//   hazard_gallery <rvhl-bundle-dir> <output-dir> [scale]
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "roados/rvhl/Rvhl.h"

using namespace roados;
using namespace roados::rvhl;
namespace fs = std::filesystem;

namespace {

constexpr float kCell = 152;  // 128 icon + label space
constexpr float kIcon = 128;

struct Item {
    std::string id;
    HazardOptions options;
    std::string label;
};

/// Draws items into a grid; each icon is composed by the core and placed as SVG.
Result<Image> sheet(const Library& lib, const std::vector<Item>& items, int columns, Theme theme, float scale) {
    const int rows = (static_cast<int>(items.size()) + columns - 1) / columns;
    const bool dark = theme == Theme::Dark;
    Canvas canvas(static_cast<int>(columns * kCell + 24), static_cast<int>(rows * (kCell + 18) + 24));
    canvas.setBackground(dark ? Color("#0B1118") : Color("#F6F8FA"));
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        const Entry* e = lib.find(it.id);
        if (!e) return makeError(ErrorCode::NotFound, it.id);
        HazardOptions o = it.options;
        o.theme = theme;
        auto composed = compose(lib, *e, o);
        if (!composed) return composed.error();
        auto doc = SvgDocument::parse(composed->svg);
        if (!doc) return doc.error();
        const float x = 12 + static_cast<float>(i % static_cast<std::size_t>(columns)) * kCell + (kCell - kIcon) / 2;
        const float y = 12 + static_cast<float>(i / static_cast<std::size_t>(columns)) * (kCell + 18);
        canvas.svg(SvgShape(*doc).position(x, y).size(kIcon, kIcon));
        canvas.text(Text(it.label.empty() ? e->id : it.label)
                        .position(x + kIcon / 2, y + kIcon + 14)
                        .family("Inter, 'Helvetica Neue', Arial, sans-serif")
                        .size(12)
                        .weight(700)
                        .anchor(TextAnchor::Middle)
                        .baseline(TextBaseline::Central)
                        .fill(dark ? Color("#C9D4DE") : Color("#333D47")));
        canvas.text(Text(e->name)
                        .position(x + kIcon / 2, y + kIcon + 30)
                        .family("Inter, 'Helvetica Neue', Arial, sans-serif")
                        .size(10)
                        .anchor(TextAnchor::Middle)
                        .baseline(TextBaseline::Central)
                        .fill(dark ? Color("#8EA0B0") : Color("#51606E")));
    }
    return canvas.render({.scale = scale});
}

int save(const Result<Image>& img, const fs::path& file) {
    if (!img) {
        std::fprintf(stderr, "%s: %s\n", file.string().c_str(), img.error().message.c_str());
        return 1;
    }
    if (!img->savePNG(file)) return 1;
    std::printf("wrote %s (%dx%d)\n", file.string().c_str(), img->width(), img->height());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: hazard_gallery <rvhl-bundle-dir> <output-dir> [scale]\n");
        return 2;
    }
    auto lib = Library::load(argv[1]);
    if (!lib) {
        std::fprintf(stderr, "%s\n", lib.error().message.c_str());
        return 1;
    }
    const fs::path out = argv[2];
    const float scale = argc > 3 ? std::stof(argv[3]) : 2.0f;
    fs::create_directories(out);

    // One entry per family, in catalogue order (no entries are invented or renamed).
    std::vector<Item> families;
    for (const auto& f : lib->families())
        if (auto members = lib->entriesInFamily(f.id); !members.empty()) families.push_back({members.front()->id, {}, {}});

    std::vector<Item> severity;
    for (int s = 1; s <= 5; ++s) {
        HazardOptions o;
        o.severity = static_cast<Severity>(s);
        severity.push_back({"RDM-003", o, std::string("S") + std::to_string(s)});
    }

    std::vector<Item> badges;
    {
        HazardOptions a; a.speed = SpeedBadge{SpeedBadge::Kind::Advisory, 30.0, SpeedUnit::Kmh}; a.distanceM = 150;
        HazardOptions b; b.speed = SpeedBadge{SpeedBadge::Kind::Posted, 50.0, SpeedUnit::Kmh}; b.count = 3;
        HazardOptions c; c.direction = Direction::Right; c.distanceM = 1200;
        HazardOptions d; d.lane = Lane{3, 1}; d.severity = Severity::S5;
        HazardOptions e; e.representation = Representation::RoadOS;
        badges = {{"RDM-003", a, "advisory + distance"}, {"GEO-002", b, "posted + count"},
                  {"ENV-001", c, "direction"}, {"DEB-001", d, "lane, S5"}, {"RSG-013", e, "RoadOS tile"}};
    }

    int rc = 0;
    rc |= save(sheet(*lib, families, 7, Theme::Dark, scale), out / "hazard-gallery-dark.png");
    rc |= save(sheet(*lib, families, 7, Theme::Light, scale), out / "hazard-gallery-light.png");
    rc |= save(sheet(*lib, severity, 5, Theme::Dark, scale), out / "severity-scale.png");
    rc |= save(sheet(*lib, badges, 5, Theme::Dark, scale), out / "compositions.png");
    return rc;
}
