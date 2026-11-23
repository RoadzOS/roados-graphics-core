# RoadOS Graphics Core

**One graphics engine. One visual language. Multiple native platforms.**

RoadOS Graphics Core is a native C++20 vector-graphics library built on [Skia](https://skia.org). It has two separate parts:

* **A. General-purpose graphics.** Shapes, Bézier paths, SVG path data, gradients, dashes, text, images, groups and layers, transforms and blend modes. It renders to PNG, raw RGBA pixel buffers or SVG. It works without any RoadOS data.
* **B. RoadOS RVHL rendering.** It provides the catalogue of the [RoadOS Visual Hazard Studio](https://github.com/RoadzOS/roados-visual-hazard-studio) (RVHL 1.0: 267 road elements, 14 families, 191 pictograms) and composes hazard icons (frames, severity, badges) with **byte-identical** output to the Studio's own `composeIcon()`. It also includes the Studio's severity and advisory-speed engine.
* **C. Binding facade** (`roados::binding`). Plain-data specs and a scene builder. The [iOS SDK](https://github.com/RoadzOS/roados-ios-sdk) (Objective-C++) and the Android SDK (JNI) only marshal values into it, so both platforms share one implementation.

The public headers contain no Skia types, no exceptions and no platform UI dependencies.

```cpp
#include <roados/Graphics.h>
using namespace roados;

int main() {
    Canvas canvas(512, 512);
    canvas.setBackground(Color::Transparent());

    canvas.circle(Circle().center(120, 120).radius(50).fill(Color("#2196F3")));
    canvas.roundedRect(RoundedRect().position(200, 100).size(180, 120).radius(20)
                           .fill(Color("#FFFFFF")).stroke(Color("#222222"), 3));
    canvas.line(Line().from(30, 300).to(450, 300).stroke(Color("#00BCD4"), 5));

    // A real RVHL pictogram (RDM-003 large pothole), rendered by Skia's SVG module.
    auto pothole = SvgDocument::load("assets/rvhl/pictograms/roados_hazard/dark/rdm-003-large-pothole~large.svg");
    if (pothole) canvas.svg(SvgShape(*pothole).position(330, 330).size(128, 128));

    auto image = canvas.render({.scale = 2});           // Result<Image>, never throws
    if (!image) return 1;                                // image.error().message explains why
    return image->savePNG("output.png") ? 0 : 1;
}
```

```cpp
#include <roados/rvhl/Rvhl.h>
using namespace roados;

auto lib = rvhl::Library::load("assets/rvhl");         // checks the format, RVHL version and SHA-256 of each file
auto hazard = rvhl::Hazard::create(*lib, "RDM-003");   // element ID (or an unambiguous slug)
hazard->severity(rvhl::Severity::S4).distance(150).size(128, 128)
       .layer(rvhl::Layer::SeverityCode, false)         // every layer can be disabled or reordered
       .overlay(Circle().center(64, 64).radius(6).fill(Color("#00FF00")));
auto image = hazard->render({.scale = 3});
```

`Color("#2196F3")` is checked **at compile time**, so an invalid literal will not compile. Runtime strings go through `Color::parse()`, which returns a `Result`.

## Status

| Phase | State |
|---|---|
| 0 Discovery | ✅ [docs/architecture/00-discovery.md](docs/architecture/00-discovery.md) |
| 1 Architecture | ✅ [docs/architecture/01-architecture.md](docs/architecture/01-architecture.md) |
| 2 Core engine (Skia, shapes, paths, paint, text, images, PNG/SVG/raw output) | ✅ |
| 3 SVG engine + RVHL pictograms | ✅ all 762 exported pictograms render; Chrome parity tests |
| 4 Catalogue API (`rvhl::Library`: load, hash and version checks, lookup by ID or slug) | ✅ |
| 5 Hazard composition (frames, severity, badges, layers, overlays) and severity engine | ✅ byte-identical with the Studio on 1,193 golden cases; 3,204 severity cases match |
| 5b Binding facade for the SDKs; cross-compiled Skia for iOS and Android | ✅ iOS built and tested; Android Skia/NDK build written but not yet run |
| 6 Benchmarks, wider visual regression | ⏳ |
| 7 Release preparation, CI | ⏳ |

Verified on macOS 27.2 arm64 with Apple clang 17 and CMake 4.5: **132 tests pass** (Debug and Release, `-Werror`). Linux and Windows are not built yet. See [docs/build](docs/build/README.md#platform-status).

## Quick start

```bash
git clone --recurse-submodules <repo-url> && cd roados-graphics-core
scripts/bootstrap_skia.sh Release        # builds pinned Skia m158 from source (~1–2 min on M-series)
# cross targets: scripts/bootstrap_skia.sh Release ios-arm64 | iossim-arm64 | android-arm64 | android-x64
cmake --preset macos-debug && cmake --build --preset macos-debug
ctest --preset macos-debug
build/macos-debug/examples/basic_shapes out.png
```

Full instructions: [docs/build/README.md](docs/build/README.md).

## Repository layout

```
include/roados/        public API (no Skia types); rvhl/ catalogue + composition; binding/ SDK facade
src/                   implementation; src/renderer is the only scene→Skia walker
assets/rvhl/           generated RVHL bundle: catalogue.json, tokens.json, 762 pictograms, manifest.json
external/…studio       pinned Studio submodule (canonical source; never modified)
tools/rvhl-export/     exporter that runs the Studio's own glyph code
tools/reference/       Chrome reference renderer for visual regression
tools/image_diff/      PNG comparison tool
tests/                 unit · integration · rendering (pixel + Chrome parity)
examples/              basic_shapes · svg_rendering
docs/                  architecture · svg-compatibility · build
scripts/               bootstrap_skia.sh · export_rvhl.sh (--check, --golden)
third_party/nlohmann/  nlohmann/json 3.11.3 (MIT, vendored, SHA-256 9bea4c80…)
```

## Identifiers

Look elements up by **element ID** (`RDM-003`). Slugs work only when they are unique: in RVHL 1.0 seven slugs are shared by a sign and a geometry entry (for example `hairpin_bend` is both `RSG-015` and `GEO-004`). `pothole` is a pictogram key used by nine entries, not an identifier. `Library::get()` returns a `NotFound` error that lists the candidate IDs.

## Where the RVHL assets come from

The Studio stores its pictograms as **TypeScript drawing functions**, not as SVG files. `scripts/export_rvhl.sh` runs the Studio's own `drawGlyph()` against the pinned submodule commit and writes `assets/rvhl/`. It covers every frame × theme × variant combination the Studio's composer can produce, and records a SHA-256 hash for each file. The output is deterministic (`--check` regenerates and diffs it), so the C++ assets cannot silently drift from the Studio.

## Licensing

* **This repository:** no license has been chosen yet.
* **Skia:** BSD-3-Clause. **nlohmann/json:** MIT. **FreeType** (Android builds only): FreeType License. **expat:** MIT. **libpng:** PNG Reference Library License. **zlib:** zlib License. **GoogleTest** (tests only): BSD-3-Clause.
* **RVHL assets** (`assets/rvhl/`, `tests/fixtures/chrome-ref/`) are generated from the Visual Hazard Studio. That repository currently declares **"all rights reserved"**, so these assets must not be redistributed until the Studio owner grants a license.

RVHL is a RoadOS proposal. It is not ISO-certified or officially approved by any standards body.
