#pragma once
/// RVHL catalogue: the RoadOS Visual Hazard Language bundle generated from the
/// Visual Hazard Studio (assets/rvhl). The Studio remains the canonical source.
/// This library only reads the generated bundle and never invents entries.
///
///     auto lib = rvhl::Library::load("assets/rvhl");
///     if (!lib) { /* lib.error().message */ }
///     const rvhl::Entry* e = lib->find("RDM-003");       // by element ID
///     const rvhl::Entry* s = lib->find("large_pothole"); // or by an unambiguous slug
///
/// A Library is a cheap-to-copy handle to immutable, shared data. It is safe to
/// use from several threads at once. Pictogram files are read lazily, checked
/// against the manifest's SHA-256 and cached.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "roados/Color.h"
#include "roados/Result.h"
#include "roados/Svg.h"

namespace roados::rvhl {

/// Bundle format and RVHL major version this build understands.
inline constexpr std::string_view kBundleFormat = "rvhl-bundle/1";
inline constexpr int kSupportedRvhlMajor = 1;

enum class Severity { S1 = 1, S2, S3, S4, S5 };
enum class Theme { Dark, Light };

/// Frame (information class) an icon is drawn in. Names match the Studio exactly.
enum class Frame {
    WarningTriangle,       ///< warning_triangle
    ProhibitoryCircle,     ///< prohibitory_circle
    MandatoryCircle,       ///< mandatory_circle
    StopOctagon,           ///< stop_octagon
    GiveWayTriangle,       ///< give_way_triangle
    InfoRect,              ///< info_rect
    DirectionBoard,        ///< direction_board
    RoadOSHazard,          ///< roados_hazard
    RoadOSMarking,         ///< roados_marking
    RoadOSInfrastructure,  ///< roados_infrastructure
    RoadOSSignal,          ///< roados_signal
    RoadOSContext,         ///< roados_context
};

/// True for frames that mirror road-sign conventions (official colours are kept).
bool isConventionFrame(Frame f) noexcept;

const char* toString(Severity s) noexcept;  ///< "S1".."S5"
const char* toString(Theme t) noexcept;     ///< "dark" / "light"
const char* toString(Frame f) noexcept;     ///< Studio name, e.g. "roados_hazard"
std::optional<Severity> parseSeverity(std::string_view s) noexcept;
std::optional<Theme> parseTheme(std::string_view s) noexcept;
std::optional<Frame> parseFrame(std::string_view s) noexcept;

struct Family {
    std::string id;     ///< e.g. "road_damage"
    std::string code;   ///< three-letter ID prefix, e.g. "RDM"
    std::string label;  ///< human-readable name
    std::string section;
    std::string description;
    Frame frame = Frame::RoadOSHazard;
};

struct Entry {
    std::string id;    ///< "RDM-003" (unique, canonical key)
    std::string name;  ///< "Large pothole"
    std::string slug;  ///< "large_pothole". NOT unique: 7 slugs name both a sign and a geometry entry
    std::string family;
    std::string description;
    std::string standardStatus;
    Frame frame = Frame::RoadOSHazard;
    std::optional<Frame> standardFrame;  ///< convention frame, if one exists
    std::vector<std::string> variants;   ///< declared glyph variants (may be empty)

    struct SeverityInfo {
        Severity defaultLevel = Severity::S3;
        bool dynamic = false;
        std::vector<std::string> factors;  ///< factor names, e.g. "approach_speed"
    } severity;

    struct Speed {
        std::string legalLimit;  ///< e.g. "independent"
        std::string advisory;    ///< "conditional" or "never"
        std::string notes;
    } speed;

    struct Confidence {
        double minimumToDisplay = 0;
        double minimumToAlert = 0;
    } confidence;

    struct Visual {
        std::string glyph;                       ///< pictogram key (shared by several entries)
        std::optional<std::string> glyphVariant;  ///< default variant
        bool slash = false;
        std::optional<std::string> legend;       ///< text baked into the pictogram
    } visual;

    std::vector<std::string> detectionMethods;
    std::vector<std::string> tags;
    std::string version;

    /// The variant drawn when none is requested (glyphVariant), if any.
    const std::optional<std::string>& defaultVariant() const { return visual.glyphVariant; }
};

/// Subset of tokens.json used by composition. Colours are kept exactly as
/// written in the tokens (e.g. "#E35D32") so composed markup matches the Studio.
struct Tokens {
    struct SeverityStyle {
        std::string label, color, onColor;
        int priority = 0;
    };
    SeverityStyle severity[5];  ///< index 0 = S1
    const SeverityStyle& of(Severity s) const { return severity[static_cast<int>(s) - 1]; }

    struct ThemeColors {
        std::string frameFill, markingFill, infraFill, contextFill, ink, inkMuted, accentBlue, halo;
    };
    ThemeColors dark, light;
    const ThemeColors& of(Theme t) const { return t == Theme::Dark ? dark : light; }

    struct Convention {
        std::string signRed, signBlue, signGreen, signWhite, signBlack, signYellow;
        std::string signalRed, signalAmber, signalGreen, markingWhite, markingYellow, water;
    } convention;

    struct Strokes { double hairline, thin, regular, bold, frame, signBorder; } stroke{};
    struct Radii { double tile, hazard, signal, chip, pill, badge; } radius{};

    struct Typography {
        std::string family, numeralFamily, monoFamily;
        double distanceSize = 12;
    } typography;

    struct Badges {
        struct { double height, radius; std::string fill, stroke, text; } distance;
        struct { double radius, ringWidth; std::string ring, fill, text; } postedSpeed;
        struct { double width, height, radius, strokeWidth; std::string fill, stroke, text, label; } advisorySpeed;
        struct { std::string fill, ring, text; } unknownSpeed;
        struct { double radius; std::string fill, text; } count;
        struct { std::string fill, stroke, text; } chip;
    } badges;
};

struct BundleInfo {
    std::string format;           ///< "rvhl-bundle/1"
    std::string rvhlVersion;      ///< e.g. "1.0.0"
    std::string exporterVersion;
    std::string studioRepository;
    std::string studioCommit;     ///< Studio commit the bundle was generated from
    int familyCount = 0, entryCount = 0, pictogramCount = 0;
};

struct LoadOptions {
    /// Verify catalogue.json, tokens.json and every pictogram read against the
    /// manifest's SHA-256 hashes. A mismatch fails with ErrorCode::Validation.
    bool verifyHashes = true;
};

class Library {
public:
    Library() = default;

    /// Loads a bundle directory containing manifest.json, catalogue.json,
    /// tokens.json and pictograms/. Fails with:
    ///  * Io / NotFound: missing files;
    ///  * Parse: malformed JSON;
    ///  * Incompatible: unknown bundle format or RVHL major version;
    ///  * Validation: hash mismatch, or a catalogue that breaks RVHL invariants
    ///    (duplicate IDs, bad ID pattern, unknown family or frame).
    static Result<Library> load(const std::filesystem::path& bundleDir, const LoadOptions& options = {});

    bool empty() const { return !impl_; }
    const BundleInfo& info() const;
    const Tokens& tokens() const;
    const std::vector<Family>& families() const;
    const std::vector<Entry>& entries() const;

    /// Exact lookup by element ID (case-insensitive, "rdm-003" works) or by a
    /// slug that belongs to exactly one entry. Returns nullptr when nothing (or
    /// more than one entry) matches. Shared slugs such as "hairpin_bend"
    /// (RSG-015 and GEO-004) and glyph keys such as "pothole" are not
    /// identifiers; get() explains which IDs to use.
    const Entry* find(std::string_view idOrSlug) const;
    /// Like find(), but returns a NotFound error naming the candidate IDs.
    Result<const Entry*> get(std::string_view idOrSlug) const;

    std::vector<const Entry*> entriesInFamily(std::string_view familyId) const;
    /// Entries drawn with a given pictogram key (e.g. "pothole" → 9 entries).
    std::vector<const Entry*> entriesWithGlyph(std::string_view glyph) const;
    const Family* family(std::string_view familyId) const;

    /// The 64×64 pictogram markup exactly as the Studio's glyph code draws it
    /// for `frame` and `theme`. `variant` defaults to the entry's glyphVariant.
    /// Fails with NotFound if that combination was not exported (for example a
    /// convention frame the entry cannot be shown in), or InvalidArgument for
    /// an undeclared variant.
    Result<std::string> pictogramSvg(const Entry& entry, Frame frame, Theme theme,
                                     const std::optional<std::string>& variant = std::nullopt) const;
    /// Same as pictogramSvg(), parsed into a renderable document.
    Result<SvgDocument> pictogram(const Entry& entry, Frame frame, Theme theme,
                                  const std::optional<std::string>& variant = std::nullopt) const;

    const std::filesystem::path& directory() const;

    struct Impl;
    const Impl* impl() const { return impl_.get(); }

private:
    explicit Library(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
    std::shared_ptr<const Impl> impl_;
};

}  // namespace roados::rvhl
