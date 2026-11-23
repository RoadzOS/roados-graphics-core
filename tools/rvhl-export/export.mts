/**
 * RVHL bundle exporter.
 *
 * Runs the Visual Hazard Studio's OWN pure modules (catalogue builder, glyph
 * registry, palette and frame resolution) against a pinned Studio checkout,
 * and writes a deterministic asset bundle for the C++ library:
 *
 *   catalogue.json   normalised catalogue (Studio buildCatalogue output)
 *   tokens.json      byte-identical copy of src/tokens/tokens.json
 *   pictograms/<frame>/<theme>/<id>-<slug>[~<variant>].svg
 *                    the glyph code's exact output, wrapped in a 64×64 <svg>
 *   manifest.json    provenance (Studio commit, versions) + SHA-256 of every file
 *
 * Nothing is redrawn or edited: each pictogram body is the string returned by
 * the Studio's drawGlyph() for the palette that composeIcon() would use.
 *
 * Usage (from the repo root):  scripts/export_rvhl.sh
 */
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join, relative } from 'node:path';

export const EXPORTER_VERSION = '1.0.0';
export const BUNDLE_FORMAT = 'rvhl-bundle/1';

const [studio, outDir] = process.argv.slice(2);
if (!studio || !outDir) {
  console.error('usage: export.mts <studio-checkout> <out-dir>');
  process.exit(2);
}

const { buildCatalogue, RVHL_VERSION } = await import(join(studio, 'src/catalogue/build.ts'));
const { hasGlyph } = await import(join(studio, 'src/icons/glyphs/index.ts'));
const { drawGlyph, paletteFor, resolveFrame, isConventionFrame } = await import(join(studio, 'src/icons/compose.ts'));

const dataDir = join(studio, 'src/catalogue/data');
const readJson = (f: string) => JSON.parse(readFileSync(join(dataDir, f), 'utf8'));
const familyFiles = readdirSync(dataDir).filter((f) => /^[a-n]-.*\.json$/.test(f)).sort().map(readJson);
const cat = buildCatalogue(readJson('families.json'), familyFiles, readJson('references.json'), hasGlyph);

const errors = cat.issues.filter((i: { level: string }) => i.level === 'error');
if (errors.length) {
  for (const e of errors) console.error(`ERROR ${e.id}: ${e.message}`);
  process.exit(1);
}

rmSync(outDir, { recursive: true, force: true });
const files: { path: string; sha256: string; bytes: number }[] = [];
function write(rel: string, content: string) {
  const abs = join(outDir, rel);
  mkdirSync(dirname(abs), { recursive: true });
  writeFileSync(abs, content);
  files.push({ path: rel, sha256: createHash('sha256').update(content).digest('hex'), bytes: Buffer.byteLength(content) });
}

// 1. Catalogue (normalised by the Studio itself) and tokens (verbatim).
write('catalogue.json', JSON.stringify({
  spec: 'RVHL',
  version: RVHL_VERSION,
  disclaimer: 'RVHL is a RoadOS specification. It is not ISO-certified or officially approved. Severity levels and advisory speeds are RoadOS proposals.',
  families: cat.families,
  entries: cat.entries,
  references: { policy: cat.referencePolicy, documents: cat.references },
}, null, 2) + '\n');
write('tokens.json', readFileSync(join(studio, 'src/tokens/tokens.json'), 'utf8'));

// 2. Pictograms for every (frame, theme, variant) the Studio composer can produce.
const kebab = (s: string) => s.replace(/_/g, '-');
const FONT_FAMILY = JSON.parse(readFileSync(join(studio, 'src/tokens/tokens.json'), 'utf8')).typography.family as string;
let pictogramCount = 0;
for (const e of cat.entries) {
  const frames = new Set<string>(['auto', 'standard', 'roados'].map((rep) => resolveFrame(e, rep).frame));
  const variants: (string | null)[] = e.variants.length ? e.variants : [e.visual.glyphVariant ?? null];
  for (const frame of [...frames].sort())
    for (const theme of ['dark', 'light'])
      for (const variant of variants) {
        const { body, found } = drawGlyph(e.visual.glyph, variant, paletteFor(frame, theme), isConventionFrame(frame), e.visual.legend ?? null);
        if (!found) throw new Error(`${e.id}: glyph ${e.visual.glyph} missing`);
        const name = `${e.id.toLowerCase()}-${kebab(e.slug)}${variant ? `~${variant}` : ''}.svg`;
        write(`pictograms/${frame}/${theme}/${name}`,
          `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">${body}</svg>\n`);
        pictogramCount++;
      }
}

// 3. Manifest with provenance and integrity hashes.
const git = (...args: string[]) => execFileSync('git', ['-C', studio, ...args], { encoding: 'utf8' }).trim();
let studioCommit = 'unknown';
let studioDirty = false;
try {
  studioCommit = git('rev-parse', 'HEAD');
  studioDirty = git('status', '--porcelain', '--untracked-files=no').length > 0;
} catch { /* not a git checkout */ }

const manifest = {
  format: BUNDLE_FORMAT,
  rvhlVersion: RVHL_VERSION,
  exporterVersion: EXPORTER_VERSION,
  source: {
    repository: 'https://github.com/GodwiniOS/roados-visual-hazard-studio',
    commit: studioCommit,
    dirty: studioDirty,
  },
  glyphFontFamily: FONT_FAMILY,
  counts: { families: cat.families.length, entries: cat.entries.length, pictograms: pictogramCount },
  files: files.sort((a, b) => a.path.localeCompare(b.path)),
};
writeFileSync(join(outDir, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
console.log(`RVHL ${RVHL_VERSION}: ${cat.entries.length} entries, ${pictogramCount} pictograms → ${relative(process.cwd(), outDir) || outDir} (studio ${studioCommit.slice(0, 7)}${studioDirty ? ', DIRTY' : ''})`);
