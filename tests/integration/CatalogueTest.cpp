// rvhl::Library against the real generated bundle (assets/rvhl).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>

#include "roados/rvhl/Rvhl.h"

using namespace roados;
using namespace roados::rvhl;
namespace fs = std::filesystem;

namespace {
const fs::path kBundle = fs::path(ROADOS_ASSETS_DIR) / "rvhl";

const Library& lib() {
    static const Library l = [] {
        auto r = Library::load(kBundle);
        if (!r) ADD_FAILURE() << r.error().message;
        return r ? *r : Library{};
    }();
    return l;
}

fs::path copyBundle(const std::string& name) {
    const fs::path dst = fs::path(ROADOS_TEST_OUTPUT_DIR) / name;
    fs::remove_all(dst);
    fs::copy(kBundle, dst, fs::copy_options::recursive);
    return dst;
}

void overwrite(const fs::path& p, const std::string& content) {
    std::ofstream(p, std::ios::binary | std::ios::trunc) << content;
}
}  // namespace

TEST(Catalogue, LoadsTheStudioBundle) {
    ASSERT_FALSE(lib().empty());
    const auto& info = lib().info();
    EXPECT_EQ(info.format, "rvhl-bundle/1");
    EXPECT_EQ(info.rvhlVersion, "1.0.0");
    EXPECT_EQ(info.studioCommit.size(), 40u);
    EXPECT_EQ(lib().entries().size(), 267u);
    EXPECT_EQ(lib().families().size(), 14u);
    EXPECT_EQ(info.pictogramCount, 762);
}

TEST(Catalogue, LooksUpByIdOrSlug) {
    const Entry* byId = lib().find("RDM-003");
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->name, "Large pothole");
    EXPECT_EQ(byId->slug, "large_pothole");
    EXPECT_EQ(byId->frame, Frame::RoadOSHazard);
    EXPECT_EQ(lib().find("rdm-003"), byId);
    EXPECT_EQ(lib().find("large_pothole"), byId);
    EXPECT_EQ(lib().find("RDM-999"), nullptr);
}

TEST(Catalogue, GlyphKeyIsNotAnIdentifier) {
    EXPECT_EQ(lib().find("pothole"), nullptr);
    auto r = lib().get("pothole");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::NotFound);
    EXPECT_NE(r.error().message.find("RDM-003"), std::string::npos) << r.error().message;
    EXPECT_EQ(lib().entriesWithGlyph("pothole").size(), 9u);
}

TEST(Catalogue, SharedSlugIsAmbiguous) {
    // The Studio does not require unique slugs: RSG-015 (sign) and GEO-004 (geometry) are both "hairpin_bend".
    EXPECT_EQ(lib().find("hairpin_bend"), nullptr);
    auto r = lib().get("hairpin_bend");
    ASSERT_FALSE(r);
    EXPECT_NE(r.error().message.find("RSG-015"), std::string::npos) << r.error().message;
    EXPECT_NE(r.error().message.find("GEO-004"), std::string::npos) << r.error().message;
    EXPECT_EQ(lib().find("GEO-004")->slug, "hairpin_bend");
}

TEST(Catalogue, FamiliesAndEntriesAreConsistent) {
    std::size_t total = 0;
    for (const auto& f : lib().families()) {
        const auto members = lib().entriesInFamily(f.id);
        EXPECT_FALSE(members.empty()) << f.id;
        for (const Entry* e : members) EXPECT_EQ(e->id.substr(0, 3), f.code);
        total += members.size();
    }
    EXPECT_EQ(total, 267u);
}

TEST(Catalogue, EveryEntryHasItsDefaultPictogram) {
    for (const auto& e : lib().entries()) {
        for (auto rep : {Representation::Auto, Representation::Standard, Representation::RoadOS}) {
            const Frame f = resolveFrame(e, rep);
            for (auto theme : {Theme::Dark, Theme::Light}) {
                auto svg = lib().pictogramSvg(e, f, theme);
                ASSERT_TRUE(svg) << svg.error().message;
                EXPECT_EQ(svg->rfind("<svg", 0), 0u);
            }
        }
    }
}

TEST(Catalogue, RejectsUndeclaredVariant) {
    const Entry* e = lib().find("RDM-003");
    auto r = lib().pictogramSvg(*e, Frame::RoadOSHazard, Theme::Dark, std::string("gigantic"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::InvalidArgument);
}

TEST(Catalogue, ConventionFrameNotExportedForRoadOSOnlyEntry) {
    const Entry* e = lib().find("RDM-003");
    auto r = lib().pictogramSvg(*e, Frame::StopOctagon, Theme::Dark);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::NotFound);
}

TEST(Catalogue, DetectsTamperedPictogram) {
    const fs::path dir = copyBundle("tampered-bundle");
    overwrite(dir / "pictograms/roados_hazard/dark/rdm-003-large-pothole~large.svg", "<svg/>");
    auto l = Library::load(dir);
    ASSERT_TRUE(l) << l.error().message;  // pictograms are verified when read
    auto r = l->pictogramSvg(*l->find("RDM-003"), Frame::RoadOSHazard, Theme::Dark);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code, ErrorCode::Validation);

    LoadOptions noVerify;
    noVerify.verifyHashes = false;
    auto unchecked = Library::load(dir, noVerify);
    ASSERT_TRUE(unchecked);
    EXPECT_TRUE(unchecked->pictogramSvg(*unchecked->find("RDM-003"), Frame::RoadOSHazard, Theme::Dark));
}

TEST(Catalogue, DetectsTamperedCatalogue) {
    const fs::path dir = copyBundle("tampered-catalogue");
    std::ifstream in(dir / "catalogue.json");
    std::string text((std::istreambuf_iterator<char>(in)), {});
    text.replace(text.find("Large pothole"), 13, "Large pothold");
    overwrite(dir / "catalogue.json", text);
    auto l = Library::load(dir);
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code, ErrorCode::Validation);
}

TEST(Catalogue, RejectsIncompatibleBundleFormat) {
    const fs::path dir = copyBundle("future-bundle");
    std::ifstream in(dir / "manifest.json");
    std::string text((std::istreambuf_iterator<char>(in)), {});
    text.replace(text.find("rvhl-bundle/1"), 13, "rvhl-bundle/2");
    overwrite(dir / "manifest.json", text);
    auto l = Library::load(dir);
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code, ErrorCode::Incompatible);
}

TEST(Catalogue, MissingBundleIsNotFound) {
    auto l = Library::load(fs::path(ROADOS_TEST_OUTPUT_DIR) / "no-such-bundle");
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code, ErrorCode::NotFound);
}

TEST(Catalogue, ConcurrentPictogramAccessIsSafe) {
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&, t] {
            for (std::size_t i = static_cast<std::size_t>(t); i < lib().entries().size(); i += 3) {
                const Entry& e = lib().entries()[i];
                if (!lib().pictogram(e, resolveFrame(e, Representation::Auto), Theme::Dark)) ++failures;
            }
        });
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}
