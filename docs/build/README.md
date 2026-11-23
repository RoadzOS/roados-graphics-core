# Building RoadOS Graphics Core

These steps were verified on macOS 15.7.3 (arm64) with Apple clang 17.0.0, CMake 4.1.2 and Python 3.13. The Linux code paths exist but have **not been built yet** (see "Platform status").

## Prerequisites

| Tool | Why | Version used |
|---|---|---|
| C++20 compiler | library | Apple clang 17 |
| CMake ≥ 3.24 | build | 4.1.2 |
| Python 3 | Skia's `gn`/`ninja` fetch scripts | 3.13 |
| git | sources | any |
| Node ≥ 18 | **only** to regenerate `assets/rvhl` from the Studio | 22.18 |
| Google Chrome | **only** to regenerate visual-regression references | 157 |

Disk: about 350 MB for Skia sources and dependencies, plus about 60 MB per Skia build configuration.

## 1. Clone

```bash
git clone --recurse-submodules <repo-url> roados-graphics-core
cd roados-graphics-core
```

The `external/roados-visual-hazard-studio` submodule is needed only when regenerating the RVHL bundle. The committed `assets/rvhl/` is enough to build and test.

## 2. Build Skia (once)

```bash
scripts/bootstrap_skia.sh Release
```

This script:

* fetches Skia `chrome/m158` at the pinned commit `37e919d8669e`, and its `expat`, `libpng` and `zlib` dependencies at the commits pinned in Skia's `DEPS`;
* downloads `gn` and `ninja` with Skia's own scripts from Google's CIPD server (ninja is SHA-256 verified);
* builds static libraries into `third_party/skia/out/Release-<os>-<cpu>/`.

**No prebuilt Skia binaries are used.** On an M-series Mac the build takes about 40 s.

Skia configuration (CPU raster, PNG, SVG, platform fonts):

```
is_official_build=true  skia_enable_svg=true  skia_use_expat=true
skia_use_libpng_{decode,encode}=true  skia_use_system_{expat,libpng,zlib}=false
skia_enable_{ganesh,graphite,pdf,skottie}=false  skia_use_{harfbuzz,icu}=false
skia_use_{libjpeg_turbo,libwebp}_*=false  skia_use_partition_alloc=false
```

To build against a different Skia checkout, set `-DROADOS_SKIA_DIR=… -DROADOS_SKIA_OUT=…`.

## 3. Configure, build, test

```bash
cmake --preset macos-debug          # or macos-release
cmake --build --preset macos-debug
ctest --preset macos-debug          # 97 tests: unit, integration, rendering
```

Presets enable `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.

## 4. Run the examples

```bash
build/macos-debug/examples/basic_shapes out.png                 # writes out.png and out.png.svg
build/macos-debug/examples/svg_rendering \
  "assets/rvhl/pictograms/roados_hazard/dark/rdm-003-large-pothole~large.svg" pothole.png 256
```

## Using the library from CMake

```cmake
add_subdirectory(roados-graphics-core)
target_link_libraries(my_app PRIVATE RoadOS::graphics)
```

Skia is a **private** dependency. Consumers include only `<roados/...>` headers, and Skia's static libraries are linked automatically.

## Maintainer workflows

| Task | Command |
|---|---|
| Regenerate the RVHL bundle after bumping the Studio submodule | `scripts/export_rvhl.sh` |
| Check that the committed bundle matches the pinned Studio (CI) | `scripts/export_rvhl.sh --check` |
| Regenerate Chrome references | `tools/reference/render_chrome_refs.sh` |
| Compare two PNGs | `build/…/tools/image_diff ref.png out.png sheet.png` |

## Platform status

| Platform | Status |
|---|---|
| macOS arm64 | ✅ built and tested (97/97) |
| macOS x64 | expected to work (same scripts); not yet built |
| Linux | code paths written (FreeType + fontconfig font manager, `FindSkia` Linux branch), **not yet built or tested** |
| Windows | not started (needs the DirectWrite font manager and `.lib` names in `FindSkia`) |
| Android NDK / iOS | architecture prepared (see `docs/architecture/01-architecture.md §9`); cross-compiling Skia not yet done |
