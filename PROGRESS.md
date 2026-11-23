# Progress log

## 2026-11-24: Phases 0–3

### Done (verified)

* **Phase 0, discovery** ([doc](docs/architecture/00-discovery.md))
  * Cloned the Studio at `f6b145e` and ran its validator (267 entries, 14 families, 0 issues).
  * Ran a census of 26,486 Studio-generated SVGs.
  * Key findings:
    * pictograms are TypeScript functions, not files;
    * glyph colours depend on frame and theme;
    * every text node uses `dominant-baseline="central"`;
    * `"pothole"` is a glyph key, not an element ID (use `RDM-003`).
* **Phase 1, architecture** ([doc](docs/architecture/01-architecture.md))
  * Seven ADRs: retained scene, no Skia in headers, `Result<T>` with no exceptions, Skia built from source, generated asset bundle, ported composition with parity tests, and text fidelity.
* **Phase 2, core engine**
  * Built Skia m158 from source (minimal dependencies).
  * CMake + `FindSkia` (defines read from `gn desc`) + presets.
  * `Color` (compile-time literals), `Transform`, `Path` (lossless with Skia: arcs become conics), and 10 shape types.
  * Brushes (solid, linear and radial gradients), strokes (caps, joins, dashes), opacity and blend modes.
  * Groups and layers (transform, opacity, clip, visibility, reorder), text (CSS family fallback, anchors, baselines), images.
  * Output to PNG, raw RGBA (`renderInto` with stride), SVG (`SkSVGCanvas`), and scaled high-resolution rendering.
* **Phase 3, SVG engine**
  * `SvgDocument` / `SvgShape` on `SkSVGDOM` with the CoreText shaper.
  * A documented text normalisation that fixes three measured Skia deviations.
  * The RVHL exporter writes 762 pictograms plus the catalogue, tokens and a hashed manifest, deterministically.
  * Visual regression against Chrome 157:
    * 13 geometry fixtures are pixel-identical (mean ≤ 0.002);
    * 6 text fixtures fall within the documented tolerances.
* **Tests:** 97/97 pass on macOS arm64, in both Debug and Release presets, with `-Werror`.

### Decisions worth knowing

* Element lookup will key on **element ID** (`RDM-003`), with slug as an alias. Glyph names are not unique (`pothole` is shared by 9 elements).
* Text normalisation changes `y`, not `dy`, because Skia applies `dy` to the first glyph only.
* Text tolerance against Chrome: mean ≤ 3.0 and ≤ 4 % of pixels over 16. Placement matches; Chrome's text gamma makes glyphs about 13 % heavier.

### Open items and blockers

| Item | Owner | Notes |
|---|---|---|
| **Studio license** | Studio owner | The Studio is "all rights reserved". `assets/rvhl` must not be published until it is licensed. |
| **This repository's license** | RoadzOS | Not chosen yet, so there is no LICENSE file. |
| GitHub repository `RoadzOS/roados-graphics-core` | RoadzOS | Does not exist yet. Work is in a local git repository. |
| Linux, Windows and CI builds | — | Linux code paths are written but not built. CI is not set up yet. |
| Text gamma parity with Chrome | — | Optional improvement. |

### Next: Phase 4

* `rvhl::Library::load(bundleDir)`:
  * check the manifest format and RVHL version;
  * verify SHA-256 hashes (on by default in tests);
  * parse `catalogue.json` and `tokens.json`.
* Look up entries by ID or slug, and enumerate them by family.
* Resolve the pictogram for each frame, theme and variant, with a thread-safe SVG cache.
* Add a JSON parser dependency (nlohmann/json, pinned by SHA-256) and a SHA-256 implementation.
