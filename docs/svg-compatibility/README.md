# SVG compatibility

**Scope:** SVG as used by RVHL 1.0, plus the SVG input that `SvgDocument` accepts in general.
**Renderer:** Skia m158 `modules/svg` (`SkSVGDOM`), with the RoadOS text-normalisation pass.
**Evidence:**
* a census of 26,486 SVGs emitted by the Studio (`tools/rvhl-export/census.mts`),
* all 762 exported pictograms rendered in `tests/integration/RvhlAssetsTest.cpp`,
* pixel comparison against Chrome in `tests/rendering/VisualRegressionTest.cpp`.

We make **no claim of full SVG 1.1 or SVG 2 support.** The matrix lists what is tested.

## Feature matrix

| Feature | RVHL uses | Status | Evidence |
|---|---|---|---|
| `<svg>` `viewBox`, `width`, `height` | yes | ✅ supported | all 762 pictograms; `Svg.ParsesSizeAndViewBox` |
| `<path d>`: M L H V C S Q T A Z, absolute and relative | yes | ✅ supported (Chrome-identical) | `geometry` fixtures, mean error 0.000 |
| `<rect>` (+`rx`), `<circle>` | yes | ✅ supported | fixtures |
| `<g transform>`: translate / scale / rotate / matrix | yes | ✅ supported | `hairpin-bend` (matrix), `cargo-spillage` (rotate), `large-stone` (scale) |
| `fill` / `stroke` as `#RRGGBB`, `none` | yes | ✅ supported | all fixtures |
| `stroke-width` / `-linecap` / `-linejoin` | yes | ✅ supported | all fixtures |
| `stroke-dasharray` | yes | ✅ supported | `unprotected-embankment` |
| `opacity`, `fill-opacity` | yes | ✅ supported | `broken-glass` |
| `<text>` with `x`, `y`, `font-size`, `font-weight` (500–900), `fill`, `text-anchor` | yes | ✅ supported | 6 text fixtures |
| `font-family` **list** (CSS fallback) | yes | ⚠️ **Skia: unsupported**; ✅ resolved by normalisation | `Svg.NormalisationShiftsYAndResolvesFamily` |
| `dominant-baseline="central"` | yes (every text node) | ⚠️ **Skia: unsupported**; ✅ emulated by normalisation | `RvhlAssets.EveryPictogramParsesAndRenders` (all text nodes adjusted) |
| `dominant-baseline` other values, or set on `<g>` | no | ❌ not emulated; reported in `SvgReport::warnings` | `Svg.NormalisationReportsWhatItCannotHandle` |
| `dy` with several characters | no | ⚠️ Skia shifts only the first glyph (not cumulative, contrary to the spec) | normalisation never emits `dy` |
| `title`, `role`, `aria-*` | yes | ignored (no visual effect) | |
| Gradients, `clipPath`, `mask`, `pattern`, `filter` | no | parsed by Skia, **untested here** | |
| `<image>`, external `href`, `<use>` | no | **untested**. No resource provider is configured, so external files are never fetched | |
| CSS `<style>`, `class` | no | Skia supports only a subset; **untested** | |

## Text normalisation (exactly what changes)

`SvgDocument::parse()` rewrites **start tags only**, and only these attributes:

1. **`font-family`.** A CSS list is replaced by the first installed family. The resolution uses `Renderer::measure()`, the same font manager that renders the text. Skia would otherwise treat `"Inter, 'Segoe UI', Roboto, Arial, sans-serif"` as one family name and fall back to the system default.
2. **`dominant-baseline="central"` on `<text>`.** The attribute is removed and `y` is increased by `(ascent − descent) / 2` of the resolved font at the element's `font-size` and `font-weight`.
   * This is Chrome's formula. We measured it in headless Chrome 157 as 0.350 em for Arial, Helvetica and the RVHL family list, which matches Arial's `(ascent − descent)/2` of 0.3466 em.
   * We change `y` rather than adding `dy` because Skia m158 applies `dy` to the first glyph only.

Nothing else is touched. Path data, shapes, colours and transforms pass through byte-for-byte; `Svg.NormalisationLeavesPathsByteIdentical` checks this. Every change is counted in `SvgReport`, and `SvgOptions{.normalizeText = false}` disables the pass.

## Parity with Chrome (the Studio's own rasteriser)

The Studio exports PNGs by drawing its SVG through the browser. We therefore compare against headless **Chrome 157.0.8103.79** (`--disable-gpu`) on macOS 15.7.3 arm64, at 128×128 px, with references in `tests/fixtures/chrome-ref/`.

The per-pixel error is the maximum channel difference on premultiplied RGBA (0–255). The pixel tolerance is 16.

| Class | Fixtures | Measured worst | Test threshold |
|---|---|---|---|
| geometry (no text) | 13 | mean 0.002, max 22, 0.006 % of pixels over tolerance | mean ≤ 0.25, ≤ 0.25 % over |
| text | 6 | mean 2.654, 3.271 % over (STOP, weight 900) | mean ≤ 3.0, ≤ 4.0 % over |

* **Geometry** is effectively pixel-identical: 12 of 13 fixtures have a max error ≤ 3. This is expected, because Chrome rasterises with Skia too.
* **Text** placement matches: ink centroids agree within 0.3 px at 128 px. The residual difference is glyph *coverage*, about 13 % more ink in Chrome. Chrome's Skia build applies its own text gamma and contrast on macOS, while standalone Skia uses its defaults.
  * **Open follow-up:** tune text gamma to match Chrome, or render text as outlines for cross-platform determinism.
* **Platform note:** text references depend on installed fonts. macOS resolves the RVHL list to Arial and DIN Alternate. On other platforms the text fixtures are skipped, and geometry fixtures still run.

## Known Skia SVG deviations (m158)

* `dominant-baseline` is not parsed (`modules/svg/src/SkSVGAttributeParser.cpp` has no parser for it).
* `font-family` is passed to `SkFontMgr::legacyMakeTypeface()` as one string. CSS lists are not split.
* `dy` is applied per glyph without accumulation (`SkSVGText.cpp`, `computeGlyphXform`).
