# Phase 0: Source project discovery

**Subject:** `GodwiniOS/roados-visual-hazard-studio`, commit `f6b145e` (the repository's only commit, no tags)
**Date:** 2026-11-24
**Method:** We cloned the repository, read the source, ran `npm run validate`, and ran a feature census script against the Studio's own modules (`tools/rvhl-export/census.mts`). That script rendered 26,486 SVGs with the canonical code. Every number below comes from those runs. None is copied from the README.

---

## 1. What the Studio is

It is a React 18 + Vite + TypeScript application. The parts that matter to us have **no UI dependency** and run under Node:

| Module | Role | Pure? |
|---|---|---|
| `src/catalogue/schema.ts` | Zod schemas: raw entry, family defaults, normalised entry, reference doc | yes |
| `src/catalogue/build.ts` | `buildCatalogue()`: merges family defaults and validates. `normaliseEntry()` | yes |
| `src/catalogue/data/*.json` | `families.json` (14), `references.json` (15 docs), `a-…n-…json` (267 entries) | data |
| `src/tokens/tokens.json` | Design tokens `rvhl-tokens/1.0`: severity, convention colours, shapes, stroke, radius, icon geometry, typography, badges, themes | data |
| `src/icons/glyphs/*.ts` | **191 pictograms, written as TypeScript functions** `(ctx) => svgString` | yes |
| `src/icons/compose.ts` | `composeIcon()`: frame, pictogram, severity, badges, returning an SVG string | yes |
| `src/engine/severity.ts` | `computeSeverity()`, `computeAdvisory()`, `stoppingDistanceM()` | yes |
| `src/export/*` | Browser download, `svgToPng()` (HTML canvas), ZIP bundles, manifests (FNV-1a) | browser |

`npm run validate` passes: **267 entries, 14 families, 15 references, 0 issues** (no missing pictograms).

## 2. Actual catalogue structure

* **Identity.** `id` matches `^[A-Z]{3}-\d{3}$`, and its prefix must equal the family `code` (SIG, RSG, MRK, INF, GEO, RDM, SPD, TMP, DEB, ENV, MOV, VIS, BRG, VEH). `slug` is `[a-z0-9_]+`, derived from `name` when absent.
* **Two-level data.** A raw entry is concise, and `normaliseEntry()` fills the gaps from its family's defaults (frame, standardStatus, severity default and dynamic flag, factors, escalation, speed rules, detection methods, confidence thresholds, dimensions, references).
* **Normalised entry fields:** `id, name, slug, family, description, standardStatus, frame, standardFrame, variants[], severity{default,dynamic,factors[]}, escalation[], speed{legalLimit,advisory,notes?}, detectionMethods[], confidence{minimumToDisplay,minimumToAlert}, dimensions, visual{glyph, glyphVariant, slash, legend, asset}, references[], tags[], version`.
* **`visual.glyph`** is the pictogram key into the glyph registry. It is **not** the element ID. Several elements share a glyph and differ by `glyphVariant`.
* `visual.asset` is a *path name* (`symbols/road-damage/rdm-003-large-pothole.svg`). **No file exists at that path in the repository**. It is the file name the Export Centre gives a generated download.
* Counts: 96 entries declare variants, for 323 (entry, variant) combinations in total. 52 have a `standardFrame`, 2 have `slash`, 5 have a `legend`.
* Frames in use: `roados_hazard` 173, `warning_triangle` 20, `roados_marking` 20, `roados_infrastructure` 17, `roados_signal` 12, `prohibitory_circle` 9, `roados_context` 8, `info_rect` 5, `stop_octagon` 1, `give_way_triangle` 1, `direction_board` 1.

### The prompt's example identifier

`RoadOS::hazard("pothole")` would **not** resolve. `pothole` is a glyph key used by 9 elements:
`RDM-001 small_pothole`, `RDM-002 medium_pothole`, `RDM-003 large_pothole`, `RDM-004 deep_pothole`, `RDM-005 water_filled_pothole`, `RDM-006 multiple_potholes`, `RDM-007 long_pothole`, `RDM-008 edge_pothole`, `ENV-004 water_covered_pothole`.
The C++ API therefore looks up by **element ID** (`"RDM-003"`), with an exact slug lookup (`"large_pothole"`) as a convenience.

## 3. Actual SVG asset structure

**The 191 pictograms are not stored as SVG files.** Each one is a function in `src/icons/glyphs/*.ts`:

```ts
interface GlyphCtx { variant: string|null; p: GlyphPalette; standard: boolean; legend: string|null; font: string }
type GlyphFn = (c: GlyphCtx) => string   // returns SVG markup on a 64×64 grid
```

The output depends on four inputs:

1. **Palette.** `paletteFor(frame, theme)` gives 16 named colours (`ink, field, muted, hole, red, …`). One glyph therefore draws in different colours on a warning triangle (black on white), a mandatory circle (white on blue) and a dark or light RoadOS tile.
2. **Variant.** Variants change the geometry, for example pothole `small|medium|large|deep|water_filled|multiple|long|edge`.
3. **`standard`.** Some glyphs omit their own sign ring when drawn inside a convention frame.
4. **`legend`.** Text such as "STOP", "BUS ONLY" or a speed value.

The finished SVG files exist only as outputs of `composeIcon()` and `pictogramSvg()`, produced in the browser at export time.

**Consequence:** the C++ library cannot consume "the SVG files" directly, because there are none. The only faithful way to obtain them is to **execute the Studio's own glyph code** and capture its output. Re-drawing them in C++ would violate "do not recreate the pictograms manually".

## 4. Existing rendering and composition approach

`composeIcon(options)` returns a 128×128 SVG string:

```
[background rect] → frame.back → <g translate/scale>pictogram</g> → [slash] → frame.front (severity code tab)
  → [convention severity bar] → [lane chip | direction chip] → [count chip] → [size chip] → [speed badge] → [distance pill]
```

* The layout constants live in code, not tokens: `GLYPH_BOX` (pictogram centre and box per frame), frame geometry, badge anchors (count at (113,17), speed at (104,82), distance pill at y=109, size chip at (2,78), direction and lane at the top left). RVHL spec §8 calls these constants *"the layout specification"* for the C++ SDK.
* **Severity treatment:**
  * RoadOS frames: the border colour equals the severity colour, plus a code tab ("S4") at the top centre.
  * Convention frames: official colours stay untouched, and a severity bar sits below the sign.
* **Representation** (`auto | standard | roados`) chooses between the convention frame and the RoadOS tile (`resolveFrame`).
* **Severity computation** (`engine/severity.ts`) starts from the default level and adds a factor delta (each factor −2…+2), clamped to ±2 and to S1–S5. Confidence gates display and alerting only.
* **Advisory speed:** S2→40, S3→30, S4→20, S5→10 km/h, capped by the posted limit, never shown as a legal limit.
* **Export:** SVG strings are rasterised by the browser (`<img>` drawn onto an HTML canvas) at 64, 128 or 192 px. The manifests use FNV-1a hashes.

## 5. SVG compatibility requirements (measured)

Census over 26,486 SVGs: 267 entries × variants × 2 themes × 2 representations × 5 severities, plus four badge permutations and the master pictograms.

| Feature | Used | Count | Skia SVG module (m158) |
|---|---|---|---|
| `svg` with `viewBox`, `width`, `height` | yes | 26,486 | supported |
| `title`, `role`, `aria-label` | yes | 26,486 | ignored (no visual effect) |
| `g transform` | yes | 81,228 | supported |
| transform `translate / scale / rotate / matrix` | yes | 76k / 49k / 4.6k / 656 | supported |
| `path d` (M L H V C S Q T A Z, absolute and relative) | yes | 95,962 | supported |
| `rect` (+`rx`), `circle` | yes | 121,920 / 47,398 | supported |
| `fill` / `stroke` as `#RRGGBB` or `none` | yes | (no other forms found) | supported |
| `stroke-width`, `stroke-linecap`, `stroke-linejoin` | yes | 64k / 48k / 57k | supported |
| `stroke-dasharray` | yes | 8,018 | supported |
| `opacity`, `fill-opacity` | yes | 984 / 82 | supported |
| `text` + `font-family` / `font-size` / `font-weight` / `fill` / `text-anchor` | yes | 76,452 | supported |
| `font-family` **fallback lists** (`Inter, 'Segoe UI', Roboto, Arial, sans-serif`) | yes | all text | **needs verification (Phase 3)** |
| `dominant-baseline="central"` | yes | **all 76,452 text nodes** | **NOT supported.** No parser for it exists in `modules/svg` |
| `font-weight` 500, 700, 800, 900 | yes | | requires matching font faces |
| gradients, clipPath, mask, pattern, filter, image, use, href, id, style, class | **no** | 0 | not needed |

**Main risk: text.** Every numeral and label (severity code, speed values, distance, "×3", "STOP", legends) relies on `dominant-baseline="central"`. Skia would place those labels on the alphabetic baseline, about 0.35 em too high. Font resolution also differs by platform: Inter is not installed on this Mac, so Chrome falls back to Arial for labels and uses DIN Alternate (a macOS system font) for numerals.

The plan, decided in Phase 3 with measured evidence:

* Implement a documented RVHL text-normalisation step that converts `central` into an equivalent `dy` from the resolved font metrics. **No path data is touched.**
* Resolve the CSS family list ourselves.
* Optionally bundle OFL-licensed fonts for deterministic, cross-platform output.

## 6. Reuse vs conversion

| Studio artefact | Use in C++ | Conversion |
|---|---|---|
| `families.json`, `references.json`, `a…n-*.json` | normalised by the Studio's own `buildCatalogue()` and emitted as `catalogue.json` | none. We do not re-implement normalisation in C++, to avoid divergence |
| `tokens.json` | read at runtime | none (copied byte-identical, hash-checked) |
| Glyph functions | executed by an exporter against a pinned Studio commit, producing static `.svg` files per (entry, variant, frame, theme) | **executed, not translated** |
| `compose.ts` layout | ported to the C++ composition engine (spec §8 sanctions this) | verified by parity tests against `composeIcon()` reference output |
| `severity.ts` | ported to C++ (small, deterministic) | verified by golden test vectors produced by the TypeScript code |

## 7. Environment (this machine)

macOS 15.7.3 arm64, 10 cores · Apple clang 17.0.0 · CMake 4.1.2 · Python 3.13.7 · Node 22.18.0 · git · gh (logged in as `GodwiniOS`) · Homebrew · Google Chrome (usable for headless reference rendering). Ninja is not installed (Skia's `bin/fetch-ninja` provides it).
**Disk:** 12 GiB free. We use a minimal Skia dependency set: expat, libpng and zlib only. We do not run `git-sync-deps`, which would pull Dawn, ANGLE, SwiftShader, emsdk and Vulkan.
**GitHub:** `RoadzOS/roados-graphics-core` **does not exist yet**. Work proceeds in a local git repository.

## 8. Licensing observations

* **Studio:** *"No license has been chosen yet… all rights reserved by the project owner."* The generated RVHL bundle in `assets/rvhl/` is derived from it. **The bundle must not be published until the Studio owner adds a license** (or grants one in writing to this repository).
* **Skia:** BSD-3-Clause. **expat:** MIT. **libpng:** libpng license. **zlib:** zlib license.
