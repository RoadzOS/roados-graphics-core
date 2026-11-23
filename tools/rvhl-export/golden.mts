/**
 * RVHL golden-vector generator.
 *
 * Runs the Visual Hazard Studio's OWN composeIcon(), computeSeverity() and
 * computeAdvisory() against the exported catalogue (assets/rvhl/catalogue.json)
 * and writes the results as test fixtures. The C++ port in src/rvhl must
 * reproduce every case exactly: composed SVG markup byte-for-byte, severity
 * levels, deltas, reasons and advisory speeds.
 *
 *   tests/fixtures/rvhl-golden/compose.json
 *   tests/fixtures/rvhl-golden/severity.json
 *
 * Usage (from the repo root):  scripts/export_rvhl.sh --golden
 */
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const [studio, bundleDir, outDir] = process.argv.slice(2);
if (!studio || !bundleDir || !outDir) {
  console.error('usage: golden.mts <studio-checkout> <rvhl-bundle-dir> <out-dir>');
  process.exit(2);
}

const { composeIcon } = await import(join(studio, 'src/icons/compose.ts'));
const { computeSeverity, computeAdvisory } = await import(join(studio, 'src/engine/severity.ts'));

const catalogue = JSON.parse(readFileSync(join(bundleDir, 'catalogue.json'), 'utf8'));
const manifest = JSON.parse(readFileSync(join(bundleDir, 'manifest.json'), 'utf8'));
const entries: any[] = catalogue.entries;
const byId = new Map(entries.map((e) => [e.id, e]));

// ---------------------------------------------------------------- compose
type Opts = Record<string, unknown>;
const cases: { name: string; entry: string; options: Opts; svg: string; frame: string; representation: string }[] = [];
function add(name: string, id: string, options: Opts) {
  const entry = byId.get(id);
  if (!entry) throw new Error(`unknown entry ${id}`);
  const r = composeIcon({ entry, ...options });
  if (!r.glyphFound) throw new Error(`${id}: glyph missing`);
  cases.push({ name, entry: id, options, svg: r.svg, frame: r.frame, representation: r.representation });
}

// 1. Every entry with defaults (dark, auto representation, default severity).
for (const e of entries) add(`default/${e.id}`, e.id, {});
// 2. Every entry, light theme, RoadOS representation (convention entries move onto a RoadOS tile).
for (const e of entries) add(`light-roados/${e.id}`, e.id, { theme: 'light', representation: 'roados' });
// 3. Every entry with a convention frame available, standard representation.
for (const e of entries.filter((x) => x.standardFrame)) add(`standard/${e.id}`, e.id, { representation: 'standard' });
// 4. Every declared variant.
for (const e of entries) for (const v of e.variants) add(`variant/${e.id}/${v}`, e.id, { variant: v });

// 5. Severity treatment and badges on a spread of frames.
// One entry per frame type, plus legend-bearing entries.
const SAMPLE = ['RDM-003', 'SIG-003', 'RSG-001', 'RSG-002', 'RSG-003', 'RSG-004', 'RSG-009', 'RSG-013', 'RSG-037',
  'MRK-014', 'INF-001', 'VEH-001', 'GEO-002'];
const severities = ['S1', 'S2', 'S3', 'S4', 'S5'];
const badgeSets: [string, Opts][] = [
  ['no-code', { showSeverityCode: false }],
  ['count', { count: 3 }],
  ['count-1', { count: 1 }],
  ['count-12', { count: 12 }],
  ['posted-50', { speed: { kind: 'posted', valueKmh: 50, unit: 'kmh' } }],
  ['posted-120', { speed: { kind: 'posted', valueKmh: 120, unit: 'kmh' } }],
  ['posted-mph', { speed: { kind: 'posted', valueKmh: 96.56, unit: 'mph' } }],
  ['advisory', { speed: { kind: 'advisory', valueKmh: 30, unit: 'kmh' } }],
  ['advisory-mph', { speed: { kind: 'advisory', valueKmh: 40, unit: 'mph' } }],
  ['unknown-speed', { speed: { kind: 'unknown', valueKmh: null, unit: 'kmh' } }],
  ['dist-0', { distanceM: 0 }],
  ['dist-45', { distanceM: 45 }],
  ['dist-155', { distanceM: 155 }],
  ['dist-995', { distanceM: 995 }],
  ['dist-1500', { distanceM: 1500 }],
  ['dist-12345', { distanceM: 12345 }],
  ['dist-imp-90', { distanceM: 90, unitSystem: 'imperial' }],
  ['dist-imp-800', { distanceM: 800, unitSystem: 'imperial' }],
  ['dist-imp-25000', { distanceM: 25000, unitSystem: 'imperial' }],
  ['dir-left', { direction: 'left' }],
  ['dir-ahead', { direction: 'ahead' }],
  ['dir-right', { direction: 'right' }],
  ['dir-both', { direction: 'both' }],
  ['lane-3-1', { lane: { count: 3, index: 1 } }],
  ['lane-9-7', { lane: { count: 9, index: 7 } }],
  ['lane-over-dir', { lane: { count: 2, index: 0 }, direction: 'left' }],
  ['size-M', { sizeVariant: 'M' }],
  ['size-XL', { sizeVariant: 'XL' }],
  ['background', { background: '#0B1118' }],
  ['everything', {
    count: 4, speed: { kind: 'advisory', valueKmh: 20, unit: 'kmh' }, distanceM: 320, direction: 'right',
    sizeVariant: 'L', background: '#FFFFFF', theme: 'light',
  }],
];
for (const id of SAMPLE) {
  if (!byId.has(id)) continue;
  for (const s of severities) add(`severity/${id}/${s}`, id, { severity: s });
  for (const [n, o] of badgeSets) add(`badge/${id}/${n}`, id, o);
}

// ---------------------------------------------------------------- severity
// Deterministic pseudo-random contexts (LCG), so the fixture is reproducible.
let seed = 0x5eed1234;
const rnd = () => ((seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 2 ** 32);
const pick = <T,>(xs: readonly T[]) => xs[Math.floor(rnd() * xs.length)];
const contexts = Array.from({ length: 12 }, () => ({
  physicalSize: pick(['small', 'medium', 'large'] as const),
  heightOrDepth: pick(['low', 'moderate', 'high'] as const),
  lanePosition: pick(['in_path', 'adjacent_lane', 'shoulder', 'off_road'] as const),
  approachSpeedKmh: pick([10, 25, 40, 50, 59, 60, 80, 89, 90, 120]),
  distanceM: pick([5, 20, 40, 75, 120, 200, 500]),
  visibility: pick(['good', 'reduced', 'poor'] as const),
  surface: pick(['dry', 'wet', 'flooded', 'loose'] as const),
  trafficDensity: pick(['free', 'moderate', 'heavy'] as const),
  vehicleType: pick(['two_wheeler', 'car', 'bus', 'truck'] as const),
  confidence: pick([0.3, 0.5, 0.6, 0.7, 0.8, 0.85, 0.95]),
}));
const severityCases = [];
for (const e of entries)
  for (let i = 0; i < contexts.length; i++) {
    const r = computeSeverity(e, contexts[i]);
    severityCases.push({
      entry: e.id, context: i,
      level: r.level, base: r.base, delta: r.delta, display: r.display, alert: r.alert,
      reasons: r.reasons,
      advisory: {
        none: computeAdvisory(e, r.level, null, contexts[i].confidence),
        posted30: computeAdvisory(e, r.level, 30, contexts[i].confidence),
        posted50: computeAdvisory(e, r.level, 50, contexts[i].confidence),
      },
    });
  }

mkdirSync(outDir, { recursive: true });
const header = { studioCommit: manifest.source.commit, rvhlVersion: manifest.rvhlVersion };
writeFileSync(join(outDir, 'compose.json'), JSON.stringify({ ...header, cases }) + '\n');
writeFileSync(join(outDir, 'severity.json'), JSON.stringify({ ...header, contexts, cases: severityCases }) + '\n');
console.log(`golden: ${cases.length} compose cases, ${severityCases.length} severity cases → ${outDir}`);
