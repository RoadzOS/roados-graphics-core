# Phase 1: Architecture and technical decisions

This document builds on [00-discovery.md](00-discovery.md). Each decision below is recorded as a short ADR (architecture decision record) that gives the context, the choice, and the consequences.

## 1. System overview

```
                    ┌──────────────────────── roados-graphics-core ────────────────────────┐
 Studio (canonical) │                                                                      │
 ┌──────────────┐   │  tools/rvhl-export  ──►  assets/rvhl/  (generated, hashed bundle)     │
 │ catalogue/*  │──►│  (runs Studio's own      catalogue.json · tokens.json · pictograms/   │
 │ glyphs/*.ts  │   │   buildCatalogue +       references/ (parity SVGs) · manifest.json    │
 │ compose.ts   │   │   drawGlyph/composeIcon)            │                                 │
 └──────────────┘   │  pinned git submodule               ▼                                 │
                    │                         ┌───────────────────────┐                    │
                    │                         │ roados_rvhl  (B)      │ Catalogue, Library,│
                    │                         │ include/roados/rvhl/  │ Hazard, Composition│
                    │                         └──────────┬────────────┘ Severity, Tokens   │
                    │                                    │ uses public API only            │
                    │                         ┌──────────▼────────────┐                    │
                    │                         │ roados_graphics  (A)  │ Canvas, shapes,    │
                    │                         │ include/roados/       │ Path, Paint, Text, │
                    │                         └──────────┬────────────┘ Image, Svg, Export │
                    │                                    │ private                         │
                    │                         ┌──────────▼────────────┐                    │
                    │                         │ Skia m158 (static)    │ core · svg ·       │
                    │                         │ third_party/skia      │ skshaper · png     │
                    │                         └───────────────────────┘                    │
                    └──────────────────────────────────────────────────────────────────────┘
       future:  Android SDK (JNI)  ──┐
                iOS SDK (Obj-C++)  ──┴──► link roados_graphics (+ roados_rvhl)
```

**A. `roados_graphics`.** This is the general-purpose engine, with no RoadOS knowledge. It works without any catalogue.
**B. `roados_rvhl`.** This is the RoadOS layer. It depends on A's *public* headers only and never includes Skia.

## 2. ADR-001: Retained scene, rendered by Skia

**Decision.**
* `Canvas` records a **scene**: a tree of value-type nodes (shapes, paths, text, images, SVG documents, groups). Nothing is drawn until `render()` is called.
* One private renderer walks the tree into an `SkCanvas`.
* The same walk targets a raster surface (PNG or raw pixels) or `SkSVGCanvas` (SVG output). There is **one rendering path**, not a second engine.

**Why.**
* Groups, layers and reordering are natural in a tree.
* A scene is plain data that can be inspected and tested without Skia.
* Rendering a `const` scene has no side effects, so several threads can render concurrently.
* The model maps cleanly onto future bindings: JNI and Obj-C++ can build scenes without seeing Skia.

**Consequence.** An immediate-mode `SkCanvas` is not exposed. A "draw into my own surface" entry point (`Renderer::renderInto(pixels, stride, …)`) covers platform surfaces.

## 3. ADR-002: No Skia types in public headers

* Public headers include only the standard library.
* Images use the pimpl idiom (`Image` owns an opaque implementation).
* Shapes, paths, transforms and colours are pure value types.

**Why.**
* The API and ABI stay stable across Skia upgrades.
* Consumers don't need Skia's include paths or defines.
* Bindings stay simple.

## 4. ADR-003: Error handling without exceptions

* Every fallible operation (parsing, I/O, decoding, rendering, catalogue lookup) returns `roados::Result<T>`, which holds either a value or an `Error{code, message}`. The API does not use exceptions.
* Colour literals use a `consteval` constructor, so `Color("#2196F3")` is validated **at compile time** and an invalid literal will not compile. Runtime strings go through `Color::parse()`, which returns a `Result`.

**Why.**
* Android and iOS bridges must not let C++ exceptions cross JNI or Obj-C boundaries.
* Many mobile builds use `-fno-exceptions`.
* Silent fallbacks are forbidden (for example, magenta for a bad colour).

## 5. ADR-004: Skia built from source at a pinned milestone

* **Pin:** `chrome/m158` @ `37e919d8669e…` (2026-11-20).
* **Build:**
  * `scripts/bootstrap_skia.sh` fetches Skia plus only **expat, libpng and zlib**, each at the commit pinned in Skia's `DEPS`.
  * It obtains `gn` and `ninja` through Skia's own fetchers from Google's CIPD (ninja is SHA-256 verified).
  * It builds the static libraries `skia, svg, skshaper, skresources, skcms, expat, png, zlib`.
* **Configuration:**
  * CPU raster only (Ganesh and Graphite off).
  * PNG codec only.
  * Platform font host (CoreText on Apple, FreeType + fontconfig on Linux).
  * No HarfBuzz or ICU yet. All RVHL text is ASCII plus `×`, which the CoreText and primitive shapers handle.
* **CMake integration:** `cmake/FindSkia.cmake` imports the prebuilt-from-source libraries and reads Skia's **public defines from `gn desc`** at configure time, so consumer defines never drift from the build.
* **Not chosen:**
  * vcpkg or Conan Skia ports: they lag milestones, and we don't control the configuration.
  * Prebuilt binaries: forbidden by the project rules.
  * A CMake `ExternalProject` that runs GN: it makes the slow step opaque and hard to cache in CI.

**Consequences.**
* Upgrading Skia means changing one SHA and running the tests.
* GPU backends (Metal, Vulkan, GL) are additive GN flags, added later for mobile surfaces.

## 6. ADR-005: RVHL asset distribution, a generated bundle from a pinned Studio

Options evaluated:

| Option | Divergence protection | Reproducible | Consumer needs Node? | Verdict |
|---|---|---|---|---|
| Submodule only, C++ parses Studio JSON and re-implements glyphs | none for glyphs (they are code) | yes | no | ✗ would require redrawing 191 pictograms |
| Hand-copied SVG/JSON | none | no | no | ✗ |
| Release-based asset package downloaded at build time | good | yes, if the release is pinned | no | later, once the Studio publishes releases |
| **Submodule (pinned) + generated, committed bundle** | **CI regenerates and diffs** | **yes** | **no** (only maintainers updating the pin) | **✓ chosen** |

* `external/roados-visual-hazard-studio` is a git submodule pinned to a Studio commit.
* `tools/rvhl-export` imports the Studio's **own** pure modules (`buildCatalogue`, `drawGlyph`, `paletteFor`, `composeIcon`) and writes `assets/rvhl/`:
  * `catalogue.json`: normalised by the Studio's builder. C++ does not re-implement normalisation.
  * `tokens.json`: byte-identical copy.
  * `pictograms/<frame>/<theme>/<id>-<slug>[~<variant>].svg`: the glyph code's exact output for every frame and theme combination that `resolveFrame` can produce for that entry.
  * `references/…`: canonical `composeIcon()` SVGs for a parity matrix (used by tests).
  * `manifest.json`: Studio commit, RVHL version, exporter version, and the SHA-256 of every file.
* The C++ `rvhl::Library::load()` verifies the manifest: version compatibility always, and file hashes optionally (enabled by default in tests).
* **Update workflow:** bump the submodule, run `tools/rvhl-export`, review the diff, commit.

## 7. ADR-006: Composition is ported, and parity is tested

* Frames, the severity tab and bar, and the badges are drawn natively by `roados_rvhl` through the **generic API**. This makes every layer individually enable-able, recolourable, movable and reorderable.
* All colours and sizes come from `tokens.json`.
* Geometry constants (`GLYPH_BOX`, frame paths, badge anchors) are ported from `compose.ts`, which spec §8 names as the layout specification.
* Divergence is caught by **parity tests**: native composition is compared pixel-for-pixel against Skia's rendering of the Studio's own `composeIcon()` SVG for the same options.

## 8. ADR-007: Text fidelity

Skia m158's SVG module ignores `dominant-baseline`, and the Studio uses `central` on every text node. The work happens in two steps:

* **Phase 3** measures the offset and implements a documented, test-covered text-baseline normalisation in the RVHL SVG loader. It only adjusts text `dy`, never path data.
* Font families are resolved through an explicit, ordered fallback list, using platform fonts by default. `FontCollection::registerFile()` loads bundled fonts for deterministic cross-platform output.

## 9. Ownership, threading, mobile

* **Values:** `Canvas`, shapes and `Path` are ordinary copyable values. `Image` and `SvgDocument` are cheap-to-copy handles to immutable data (ref-counted).
* **Threading:**
  * Building a scene is single-threaded per `Canvas`.
  * `render()` on a `const Canvas` is safe to call concurrently.
  * `rvhl::Library` is immutable after `load()` and safe to share. Its SVG cache is guarded internally.
* **Global state:**
  * No global mutable state is exposed.
  * The default `Renderer` holds a platform font manager created once with thread-safe static initialisation. Callers can create and own their own `Renderer`.
* **Mobile:**
  * **Android:** NDK static libraries, a Kotlin `external fun` JNI layer converting to `Result`, and `Image::pixels()` copied into an Android `Bitmap`.
  * **iOS:** an Obj-C++ wrapper with `CGImage`/`UIImage` built from raw RGBA.
  * Both bind to the `Result`-returning API only. Skia is built per ABI with the same script, using a different GN `target_os`/`target_cpu`.

## 10. Phased build plan

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 2 | CMake + Skia, Color, Transform, Path, shapes, paint, gradients, text, groups, Canvas, Image (PNG/raw), SVG output, tests, `basic_shapes` example | `ctest` green; example writes a PNG |
| 3 | `SvgDocument` (load, render into box, preserveAspectRatio), text-baseline normalisation, compatibility matrix tests on real RVHL SVGs | real RVHL pictograms render; documented tolerances |
| 4 | Submodule + exporter + bundle + `rvhl::Library` (load, validate, lookup, cache) | all 267 entries load; every pictogram renders |
| 5 | Composition engine + layers + severity/advisory port | parity tests vs `composeIcon()` |
| 6 | Visual regression (vs Chrome-rendered references), benchmarks | documented tolerances; measured numbers |
| 7 | Docs, versioning, CI | clean clone build passes in CI |

## 11. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Studio has **no license** | the generated bundle cannot be published | owner adds a license before this repo goes public |
| `dominant-baseline` unsupported | labels shifted | ADR-007 normalisation + tests |
| Fonts differ per platform (Inter is not installed) | text pixels differ | bundled OFL fonts option; text-aware tolerances |
| Skia API churn between milestones | upgrade breakage | pinned SHA; Skia isolated behind `src/renderer` |
| Glyphs are code: a Studio change silently alters an asset | divergence | manifest hashes + CI regeneration diff |
| 12 GiB free disk | full Skia sync impossible | minimal DEPS set (≈ 350 MB total) |
