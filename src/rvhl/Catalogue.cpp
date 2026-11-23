#include "roados/rvhl/Catalogue.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <mutex>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "rvhl/Sha256.h"

namespace roados::rvhl {

using nlohmann::json;
namespace fs = std::filesystem;

// ------------------------------------------------------------------ enums

namespace {
constexpr std::array<std::pair<Frame, const char*>, 12> kFrames = {{
    {Frame::WarningTriangle, "warning_triangle"},
    {Frame::ProhibitoryCircle, "prohibitory_circle"},
    {Frame::MandatoryCircle, "mandatory_circle"},
    {Frame::StopOctagon, "stop_octagon"},
    {Frame::GiveWayTriangle, "give_way_triangle"},
    {Frame::InfoRect, "info_rect"},
    {Frame::DirectionBoard, "direction_board"},
    {Frame::RoadOSHazard, "roados_hazard"},
    {Frame::RoadOSMarking, "roados_marking"},
    {Frame::RoadOSInfrastructure, "roados_infrastructure"},
    {Frame::RoadOSSignal, "roados_signal"},
    {Frame::RoadOSContext, "roados_context"},
}};
}  // namespace

bool isConventionFrame(Frame f) noexcept {
    switch (f) {
        case Frame::WarningTriangle:
        case Frame::ProhibitoryCircle:
        case Frame::MandatoryCircle:
        case Frame::StopOctagon:
        case Frame::GiveWayTriangle:
        case Frame::InfoRect:
        case Frame::DirectionBoard: return true;
        default: return false;
    }
}

const char* toString(Severity s) noexcept {
    static constexpr const char* names[] = {"S1", "S2", "S3", "S4", "S5"};
    const int i = static_cast<int>(s) - 1;
    return (i >= 0 && i < 5) ? names[i] : "S?";
}
const char* toString(Theme t) noexcept { return t == Theme::Dark ? "dark" : "light"; }
const char* toString(Frame f) noexcept {
    for (const auto& [frame, name] : kFrames)
        if (frame == f) return name;
    return "unknown";
}
std::optional<Severity> parseSeverity(std::string_view s) noexcept {
    if (s.size() == 2 && (s[0] == 'S' || s[0] == 's') && s[1] >= '1' && s[1] <= '5')
        return static_cast<Severity>(s[1] - '0');
    return std::nullopt;
}
std::optional<Theme> parseTheme(std::string_view s) noexcept {
    if (s == "dark") return Theme::Dark;
    if (s == "light") return Theme::Light;
    return std::nullopt;
}
std::optional<Frame> parseFrame(std::string_view s) noexcept {
    for (const auto& [frame, name] : kFrames)
        if (s == name) return frame;
    return std::nullopt;
}

// ------------------------------------------------------------------ impl

struct Library::Impl {
    fs::path dir;
    BundleInfo info;
    Tokens tokens;
    std::vector<Family> families;
    std::vector<Entry> entries;
    std::unordered_map<std::string, std::size_t> byId;    // upper-case ID
    std::unordered_map<std::string, std::vector<std::size_t>> bySlug;  // slugs are not unique in RVHL 1.0
    std::unordered_map<std::string, std::string> hashes;  // bundle-relative path → sha256
    bool verify = true;

    mutable std::mutex cacheMutex;
    mutable std::unordered_map<std::string, std::string> svgCache;      // path → markup
    mutable std::unordered_map<std::string, SvgDocument> docCache;      // path → parsed document

    Result<std::string> readVerified(const std::string& rel) const;
};

namespace {

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}
std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

Result<std::string> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        std::error_code ec;
        if (!fs::exists(p, ec)) return makeError(ErrorCode::NotFound, "file not found: " + p.string());
        return makeError(ErrorCode::Io, "cannot read " + p.string());
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// Field readers that never throw; the first failure is recorded in `error`.
struct Reader {
    std::string file;
    std::optional<Error> error;

    void fail(const std::string& where, const std::string& what) {
        if (!error) error = makeError(ErrorCode::Validation, file + ": " + where + ": " + what);
    }
    const json* field(const json& j, const char* key, const std::string& where) {
        if (!j.is_object()) { fail(where, "expected an object"); return nullptr; }
        auto it = j.find(key);
        if (it == j.end()) { fail(where, std::string("missing \"") + key + "\""); return nullptr; }
        return &*it;
    }
    std::string str(const json& j, const char* key, const std::string& where) {
        const json* v = field(j, key, where);
        if (!v) return {};
        if (!v->is_string()) { fail(where, std::string("\"") + key + "\" must be a string"); return {}; }
        return v->get<std::string>();
    }
    std::optional<std::string> optStr(const json& j, const char* key, const std::string& where) {
        if (!j.is_object()) return std::nullopt;
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) return std::nullopt;
        if (!it->is_string()) { fail(where, std::string("\"") + key + "\" must be a string or null"); return std::nullopt; }
        return it->get<std::string>();
    }
    double num(const json& j, const char* key, const std::string& where) {
        const json* v = field(j, key, where);
        if (!v) return 0;
        if (!v->is_number()) { fail(where, std::string("\"") + key + "\" must be a number"); return 0; }
        return v->get<double>();
    }
    bool boolean(const json& j, const char* key, const std::string& where) {
        const json* v = field(j, key, where);
        if (!v) return false;
        if (!v->is_boolean()) { fail(where, std::string("\"") + key + "\" must be a boolean"); return false; }
        return v->get<bool>();
    }
    std::vector<std::string> strings(const json& j, const char* key, const std::string& where) {
        std::vector<std::string> out;
        const json* v = field(j, key, where);
        if (!v) return out;
        if (!v->is_array()) { fail(where, std::string("\"") + key + "\" must be an array"); return out; }
        for (const auto& x : *v) {
            if (!x.is_string()) { fail(where, std::string("\"") + key + "\" must contain strings"); return out; }
            out.push_back(x.get<std::string>());
        }
        return out;
    }
    const json& obj(const json& j, const char* key, const std::string& where) {
        static const json kEmpty = json::object();
        const json* v = field(j, key, where);
        if (!v) return kEmpty;
        if (!v->is_object()) { fail(where, std::string("\"") + key + "\" must be an object"); return kEmpty; }
        return *v;
    }
    Frame frame(const std::string& name, const std::string& where) {
        auto f = parseFrame(name);
        if (!f) fail(where, "unknown frame \"" + name + "\"");
        return f.value_or(Frame::RoadOSHazard);
    }
};

Result<json> parseJson(const std::string& text, const std::string& file) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return makeError(ErrorCode::Parse, file + ": malformed JSON");
    return j;
}

void readTokens(Reader& r, const json& t, Tokens& out) {
    const json& sev = r.obj(t, "severity", "severity");
    for (int i = 0; i < 5; ++i) {
        const std::string key = std::string("S") + char('1' + i);
        const json& s = r.obj(sev, key.c_str(), "severity");
        auto& o = out.severity[i];
        o.label = r.str(s, "label", "severity." + key);
        o.color = r.str(s, "color", "severity." + key);
        o.onColor = r.str(s, "onColor", "severity." + key);
        o.priority = static_cast<int>(r.num(s, "priority", "severity." + key));
    }
    const json& themes = r.obj(t, "themes", "themes");
    for (auto [name, dst] : {std::pair{"dark", &out.dark}, std::pair{"light", &out.light}}) {
        const json& th = r.obj(themes, name, "themes");
        const std::string w = std::string("themes.") + name;
        dst->frameFill = r.str(th, "frameFill", w);
        dst->markingFill = r.str(th, "markingFill", w);
        dst->infraFill = r.str(th, "infraFill", w);
        dst->contextFill = r.str(th, "contextFill", w);
        dst->ink = r.str(th, "ink", w);
        dst->inkMuted = r.str(th, "inkMuted", w);
        dst->accentBlue = r.str(th, "accentBlue", w);
        dst->halo = r.str(th, "halo", w);
    }
    const json& cv = r.obj(t, "convention", "convention");
    auto& c = out.convention;
    c.signRed = r.str(cv, "signRed", "convention");
    c.signBlue = r.str(cv, "signBlue", "convention");
    c.signGreen = r.str(cv, "signGreen", "convention");
    c.signWhite = r.str(cv, "signWhite", "convention");
    c.signBlack = r.str(cv, "signBlack", "convention");
    c.signYellow = r.str(cv, "signYellow", "convention");
    c.signalRed = r.str(cv, "signalRed", "convention");
    c.signalAmber = r.str(cv, "signalAmber", "convention");
    c.signalGreen = r.str(cv, "signalGreen", "convention");
    c.markingWhite = r.str(cv, "markingWhite", "convention");
    c.markingYellow = r.str(cv, "markingYellow", "convention");
    c.water = r.str(cv, "water", "convention");

    const json& st = r.obj(t, "stroke", "stroke");
    out.stroke = {r.num(st, "hairline", "stroke"), r.num(st, "thin", "stroke"), r.num(st, "regular", "stroke"),
                  r.num(st, "bold", "stroke"),     r.num(st, "frame", "stroke"), r.num(st, "signBorder", "stroke")};
    const json& rd = r.obj(t, "radius", "radius");
    out.radius = {r.num(rd, "tile", "radius"), r.num(rd, "hazard", "radius"), r.num(rd, "signal", "radius"),
                  r.num(rd, "chip", "radius"), r.num(rd, "pill", "radius"),   r.num(rd, "badge", "radius")};
    const json& ty = r.obj(t, "typography", "typography");
    out.typography.family = r.str(ty, "family", "typography");
    out.typography.numeralFamily = r.str(ty, "numeralFamily", "typography");
    out.typography.monoFamily = r.str(ty, "monoFamily", "typography");
    out.typography.distanceSize = r.num(ty, "distanceSize", "typography");

    const json& b = r.obj(t, "badges", "badges");
    auto& B = out.badges;
    const json& d = r.obj(b, "distance", "badges");
    B.distance = {r.num(d, "height", "badges.distance"), r.num(d, "radius", "badges.distance"),
                  r.str(d, "fill", "badges.distance"), r.str(d, "stroke", "badges.distance"),
                  r.str(d, "text", "badges.distance")};
    const json& p = r.obj(b, "postedSpeed", "badges");
    B.postedSpeed = {r.num(p, "radius", "badges.postedSpeed"), r.num(p, "ringWidth", "badges.postedSpeed"),
                     r.str(p, "ring", "badges.postedSpeed"), r.str(p, "fill", "badges.postedSpeed"),
                     r.str(p, "text", "badges.postedSpeed")};
    const json& a = r.obj(b, "advisorySpeed", "badges");
    B.advisorySpeed = {r.num(a, "width", "badges.advisorySpeed"), r.num(a, "height", "badges.advisorySpeed"),
                       r.num(a, "radius", "badges.advisorySpeed"), r.num(a, "strokeWidth", "badges.advisorySpeed"),
                       r.str(a, "fill", "badges.advisorySpeed"), r.str(a, "stroke", "badges.advisorySpeed"),
                       r.str(a, "text", "badges.advisorySpeed"), r.str(a, "label", "badges.advisorySpeed")};
    const json& u = r.obj(b, "unknownSpeed", "badges");
    B.unknownSpeed = {r.str(u, "fill", "badges.unknownSpeed"), r.str(u, "ring", "badges.unknownSpeed"),
                      r.str(u, "text", "badges.unknownSpeed")};
    const json& cn = r.obj(b, "count", "badges");
    B.count = {r.num(cn, "radius", "badges.count"), r.str(cn, "fill", "badges.count"), r.str(cn, "text", "badges.count")};
    const json& ch = r.obj(b, "chip", "badges");
    B.chip = {r.str(ch, "fill", "badges.chip"), r.str(ch, "stroke", "badges.chip"), r.str(ch, "text", "badges.chip")};
}

Entry readEntry(Reader& r, const json& e) {
    Entry out;
    out.id = r.str(e, "id", "entry");
    const std::string w = "entry " + out.id;
    out.name = r.str(e, "name", w);
    out.slug = r.str(e, "slug", w);
    out.family = r.str(e, "family", w);
    out.description = r.str(e, "description", w);
    out.standardStatus = r.str(e, "standardStatus", w);
    out.frame = r.frame(r.str(e, "frame", w), w);
    if (auto sf = r.optStr(e, "standardFrame", w)) out.standardFrame = r.frame(*sf, w);
    out.variants = r.strings(e, "variants", w);

    const json& sev = r.obj(e, "severity", w);
    const std::string def = r.str(sev, "default", w + ".severity");
    auto lvl = parseSeverity(def);
    if (!lvl) r.fail(w, "invalid default severity \"" + def + "\"");
    out.severity.defaultLevel = lvl.value_or(Severity::S3);
    out.severity.dynamic = r.boolean(sev, "dynamic", w + ".severity");
    out.severity.factors = r.strings(sev, "factors", w + ".severity");

    const json& sp = r.obj(e, "speed", w);
    out.speed.legalLimit = r.str(sp, "legalLimit", w + ".speed");
    out.speed.advisory = r.str(sp, "advisory", w + ".speed");
    out.speed.notes = r.optStr(sp, "notes", w + ".speed").value_or("");

    const json& cf = r.obj(e, "confidence", w);
    out.confidence.minimumToDisplay = r.num(cf, "minimumToDisplay", w + ".confidence");
    out.confidence.minimumToAlert = r.num(cf, "minimumToAlert", w + ".confidence");

    const json& v = r.obj(e, "visual", w);
    out.visual.glyph = r.str(v, "glyph", w + ".visual");
    out.visual.glyphVariant = r.optStr(v, "glyphVariant", w + ".visual");
    out.visual.slash = r.boolean(v, "slash", w + ".visual");
    out.visual.legend = r.optStr(v, "legend", w + ".visual");

    out.detectionMethods = r.strings(e, "detectionMethods", w);
    out.tags = r.strings(e, "tags", w);
    out.version = r.str(e, "version", w);
    return out;
}

int majorOf(const std::string& version) {
    try {
        return std::stoi(version.substr(0, version.find('.')));
    } catch (...) {
        return -1;
    }
}

std::string kebab(std::string s) {
    std::replace(s.begin(), s.end(), '_', '-');
    return s;
}

}  // namespace

Result<std::string> Library::Impl::readVerified(const std::string& rel) const {
    if (verify) {
        auto it = hashes.find(rel);
        if (it == hashes.end()) return makeError(ErrorCode::NotFound, "not listed in the bundle manifest: " + rel);
        auto data = readFile(dir / rel);
        if (!data) return data.error();
        if (detail::sha256Hex(*data) != it->second)
            return makeError(ErrorCode::Validation, "SHA-256 mismatch (file modified or corrupted): " + rel);
        return data;
    }
    return readFile(dir / rel);
}

Result<Library> Library::load(const fs::path& bundleDir, const LoadOptions& options) {
    auto impl = std::make_shared<Impl>();
    impl->dir = bundleDir;
    impl->verify = options.verifyHashes;

    // 1. Manifest: format, version, provenance, hashes.
    auto manifestText = readFile(bundleDir / "manifest.json");
    if (!manifestText) return manifestText.error();
    auto manifest = parseJson(*manifestText, "manifest.json");
    if (!manifest) return manifest.error();
    Reader r{"manifest.json", std::nullopt};
    const json& m = *manifest;
    auto& info = impl->info;
    info.format = r.str(m, "format", "manifest");
    info.rvhlVersion = r.str(m, "rvhlVersion", "manifest");
    info.exporterVersion = r.str(m, "exporterVersion", "manifest");
    const json& src = r.obj(m, "source", "manifest");
    info.studioRepository = r.str(src, "repository", "source");
    info.studioCommit = r.str(src, "commit", "source");
    const json& counts = r.obj(m, "counts", "manifest");
    info.familyCount = static_cast<int>(r.num(counts, "families", "counts"));
    info.entryCount = static_cast<int>(r.num(counts, "entries", "counts"));
    info.pictogramCount = static_cast<int>(r.num(counts, "pictograms", "counts"));
    if (const json* files = r.field(m, "files", "manifest"); files && files->is_array()) {
        for (const auto& f : *files) impl->hashes[r.str(f, "path", "files")] = r.str(f, "sha256", "files");
    }
    if (r.error) return *r.error;
    if (info.format != kBundleFormat)
        return makeError(ErrorCode::Incompatible, "unsupported bundle format \"" + info.format + "\" (expected " +
                                                      std::string(kBundleFormat) + ")");
    if (majorOf(info.rvhlVersion) != kSupportedRvhlMajor)
        return makeError(ErrorCode::Incompatible, "unsupported RVHL version " + info.rvhlVersion + " (this build reads " +
                                                      std::to_string(kSupportedRvhlMajor) + ".x)");

    // 2. Tokens.
    auto tokensText = impl->readVerified("tokens.json");
    if (!tokensText) return tokensText.error();
    auto tokens = parseJson(*tokensText, "tokens.json");
    if (!tokens) return tokens.error();
    Reader tr{"tokens.json", std::nullopt};
    readTokens(tr, *tokens, impl->tokens);
    if (tr.error) return *tr.error;

    // 3. Catalogue (already normalised by the Studio's buildCatalogue()).
    auto catText = impl->readVerified("catalogue.json");
    if (!catText) return catText.error();
    auto cat = parseJson(*catText, "catalogue.json");
    if (!cat) return cat.error();
    Reader cr{"catalogue.json", std::nullopt};
    const std::string catVersion = cr.str(*cat, "version", "catalogue");
    if (const json* fams = cr.field(*cat, "families", "catalogue"); fams && fams->is_array()) {
        for (const auto& f : *fams) {
            Family fam;
            fam.id = cr.str(f, "id", "family");
            fam.code = cr.str(f, "code", "family " + fam.id);
            fam.label = cr.str(f, "label", "family " + fam.id);
            fam.section = cr.str(f, "section", "family " + fam.id);
            fam.description = cr.str(f, "description", "family " + fam.id);
            fam.frame = cr.frame(cr.str(f, "frame", "family " + fam.id), "family " + fam.id);
            impl->families.push_back(std::move(fam));
        }
    }
    if (const json* ents = cr.field(*cat, "entries", "catalogue"); ents && ents->is_array()) {
        impl->entries.reserve(ents->size());
        for (const auto& e : *ents) impl->entries.push_back(readEntry(cr, e));
    }
    if (cr.error) return *cr.error;
    if (catVersion != info.rvhlVersion)
        return makeError(ErrorCode::Validation,
                         "catalogue version " + catVersion + " does not match manifest " + info.rvhlVersion);

    // 4. RVHL invariants (the Studio enforces these too; a bundle that breaks
    //    them was edited by hand or produced by an incompatible exporter).
    static const std::regex idPattern("^[A-Z]{3}-[0-9]{3}$");
    std::unordered_map<std::string, std::string> familyCode;
    for (const auto& f : impl->families) familyCode[f.id] = f.code;
    for (std::size_t i = 0; i < impl->entries.size(); ++i) {
        const Entry& e = impl->entries[i];
        if (!std::regex_match(e.id, idPattern))
            return makeError(ErrorCode::Validation, "entry ID \"" + e.id + "\" does not match ^[A-Z]{3}-\\d{3}$");
        auto fc = familyCode.find(e.family);
        if (fc == familyCode.end())
            return makeError(ErrorCode::Validation, e.id + ": unknown family \"" + e.family + "\"");
        if (e.id.compare(0, 3, fc->second) != 0)
            return makeError(ErrorCode::Validation, e.id + ": ID prefix does not match family code " + fc->second);
        if (!impl->byId.emplace(e.id, i).second)
            return makeError(ErrorCode::Validation, "duplicate entry ID " + e.id);
        impl->bySlug[e.slug].push_back(i);
        if (!e.variants.empty() && e.visual.glyphVariant &&
            std::find(e.variants.begin(), e.variants.end(), *e.visual.glyphVariant) == e.variants.end())
            return makeError(ErrorCode::Validation, e.id + ": glyphVariant is not a declared variant");
    }
    if (static_cast<int>(impl->entries.size()) != info.entryCount ||
        static_cast<int>(impl->families.size()) != info.familyCount)
        return makeError(ErrorCode::Validation, "catalogue counts do not match the manifest");

    return Library(std::move(impl));
}

namespace {
const BundleInfo kEmptyInfo{};
const Tokens kEmptyTokens{};
const std::vector<Family> kNoFamilies{};
const std::vector<Entry> kNoEntries{};
const fs::path kNoPath{};
}  // namespace

const BundleInfo& Library::info() const { return impl_ ? impl_->info : kEmptyInfo; }
const Tokens& Library::tokens() const { return impl_ ? impl_->tokens : kEmptyTokens; }
const std::vector<Family>& Library::families() const { return impl_ ? impl_->families : kNoFamilies; }
const std::vector<Entry>& Library::entries() const { return impl_ ? impl_->entries : kNoEntries; }
const fs::path& Library::directory() const { return impl_ ? impl_->dir : kNoPath; }

const Entry* Library::find(std::string_view key) const {
    if (!impl_) return nullptr;
    if (auto it = impl_->byId.find(upper(key)); it != impl_->byId.end()) return &impl_->entries[it->second];
    if (auto it = impl_->bySlug.find(std::string(key)); it != impl_->bySlug.end() && it->second.size() == 1)
        return &impl_->entries[it->second.front()];
    return nullptr;
}

Result<const Entry*> Library::get(std::string_view key) const {
    if (impl_ == nullptr) return makeError(ErrorCode::InvalidArgument, "library is not loaded");
    if (const Entry* e = find(key)) return e;
    if (auto it = impl_->bySlug.find(std::string(key)); it != impl_->bySlug.end()) {
        std::string msg = "slug \"" + std::string(key) + "\" is shared by several entries; use an ID:";
        for (std::size_t i : it->second) msg += " " + impl_->entries[i].id + " (" + impl_->entries[i].name + ")";
        return makeError(ErrorCode::NotFound, msg);
    }
    std::string msg = "no RVHL entry with ID or slug \"" + std::string(key) + "\"";
    const auto byGlyph = entriesWithGlyph(lower(key));
    if (!byGlyph.empty()) {
        msg += "; \"" + std::string(key) + "\" is a pictogram key shared by:";
        for (const Entry* e : byGlyph) msg += " " + e->id + " (" + e->slug + ")";
    }
    return makeError(ErrorCode::NotFound, msg);
}

std::vector<const Entry*> Library::entriesInFamily(std::string_view familyId) const {
    std::vector<const Entry*> out;
    for (const auto& e : entries())
        if (e.family == familyId) out.push_back(&e);
    return out;
}

std::vector<const Entry*> Library::entriesWithGlyph(std::string_view glyph) const {
    std::vector<const Entry*> out;
    for (const auto& e : entries())
        if (e.visual.glyph == glyph) out.push_back(&e);
    return out;
}

const Family* Library::family(std::string_view familyId) const {
    for (const auto& f : families())
        if (f.id == familyId) return &f;
    return nullptr;
}

namespace {
Result<std::string> pictogramPath(const Entry& entry, Frame frame, Theme theme, const std::optional<std::string>& requested) {
    const std::optional<std::string>& variant = requested ? requested : entry.visual.glyphVariant;
    if (variant) {
        const bool declared = entry.variants.empty()
                                  ? (entry.visual.glyphVariant && *variant == *entry.visual.glyphVariant)
                                  : std::find(entry.variants.begin(), entry.variants.end(), *variant) != entry.variants.end();
        if (!declared) {
            std::string msg = entry.id + ": variant \"" + *variant + "\" is not declared";
            if (!entry.variants.empty()) {
                msg += " (variants:";
                for (const auto& v : entry.variants) msg += " " + v;
                msg += ")";
            }
            return makeError(ErrorCode::InvalidArgument, msg);
        }
    }
    std::string name = lower(entry.id) + "-" + kebab(entry.slug);
    if (variant) name += "~" + *variant;
    return std::string("pictograms/") + toString(frame) + "/" + toString(theme) + "/" + name + ".svg";
}
}  // namespace

Result<std::string> Library::pictogramSvg(const Entry& entry, Frame frame, Theme theme,
                                          const std::optional<std::string>& variant) const {
    if (!impl_) return makeError(ErrorCode::InvalidArgument, "library is not loaded");
    auto rel = pictogramPath(entry, frame, theme, variant);
    if (!rel) return rel.error();
    {
        std::lock_guard lock(impl_->cacheMutex);
        if (auto it = impl_->svgCache.find(*rel); it != impl_->svgCache.end()) return it->second;
    }
    auto data = impl_->readVerified(*rel);
    if (!data) {
        if (data.error().code == ErrorCode::NotFound)
            return makeError(ErrorCode::NotFound, entry.id + ": no pictogram exported for frame " + toString(frame) +
                                                      ", theme " + toString(theme) + " (" + *rel + ")");
        return data.error();
    }
    std::lock_guard lock(impl_->cacheMutex);
    return impl_->svgCache.emplace(*rel, std::move(*data)).first->second;
}

Result<SvgDocument> Library::pictogram(const Entry& entry, Frame frame, Theme theme,
                                       const std::optional<std::string>& variant) const {
    auto rel = pictogramPath(entry, frame, theme, variant);
    if (!rel) return rel.error();
    if (impl_) {
        std::lock_guard lock(impl_->cacheMutex);
        if (auto it = impl_->docCache.find(*rel); it != impl_->docCache.end()) return it->second;
    }
    auto svg = pictogramSvg(entry, frame, theme, variant);
    if (!svg) return svg.error();
    auto doc = SvgDocument::parse(*svg);
    if (!doc) return doc.error();
    std::lock_guard lock(impl_->cacheMutex);
    return impl_->docCache.emplace(*rel, std::move(*doc)).first->second;
}

}  // namespace roados::rvhl
