import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
const S = process.argv[2];
const { buildCatalogue } = await import(join(S, 'src/catalogue/build.ts'));
const { hasGlyph, GLYPHS } = await import(join(S, 'src/icons/glyphs/index.ts'));
const { composeIcon, pictogramSvg } = await import(join(S, 'src/icons/compose.ts'));
const dir = join(S, 'src/catalogue/data');
const read = (f: string) => JSON.parse(readFileSync(join(dir, f), 'utf8'));
const files = readdirSync(dir).filter((f) => /^[a-n]-.*\.json$/.test(f)).sort().map(read);
const cat = buildCatalogue(read('families.json'), files, read('references.json'), hasGlyph);
const els = new Map<string, number>(), attrs = new Map<string, number>(), xf = new Map<string, number>(), cmds = new Map<string, number>(), misc = new Map<string,number>();
const inc = (m: Map<string, number>, k: string) => m.set(k, (m.get(k) ?? 0) + 1);
let n = 0, bytes = 0, maxB = 0; const texts = new Set<string>(); const fonts = new Set<string>(); const baselines = new Set<string>();
function scan(svg: string) {
  n++; bytes += svg.length; maxB = Math.max(maxB, svg.length);
  for (const m of svg.matchAll(/<([a-zA-Z]+)([^>]*)>/g)) {
    inc(els, m[1]);
    for (const a of m[2].matchAll(/\s([a-zA-Z:-]+)="([^"]*)"/g)) {
      inc(attrs, `${m[1]}@${a[1]}`);
      if (a[1] === 'transform') for (const t of a[2].matchAll(/([a-z]+)\(/gi)) inc(xf, t[1]);
      if (a[1] === 'd') for (const c of a[2].matchAll(/[MmLlHhVvCcSsQqTtAaZz]/g)) inc(cmds, c[0]);
      if (a[1] === 'font-family') fonts.add(a[2]);
      if (a[1] === 'dominant-baseline') baselines.add(a[2]);
      if (a[1] === 'fill' && a[2].startsWith('url')) inc(misc, 'fill-url');
      if (a[1] === 'fill' && !/^#[0-9A-Fa-f]{6}$|^none$/.test(a[2])) inc(misc, 'fill-nonhex:' + a[2]);
      if (a[1] === 'stroke' && !/^#[0-9A-Fa-f]{6}$|^none$/.test(a[2])) inc(misc, 'stroke-nonhex:' + a[2]);
      if (a[1] === 'fill-rule' || a[1] === 'clip-rule') inc(misc, a[1] + '=' + a[2]);
    }
  }
  for (const t of svg.matchAll(/<text[^>]*>([^<]*)</g)) texts.add(t[1]);
  for (const k of ['<style', 'class=', 'clipPath', '<mask', 'Gradient', '<image', 'href', 'id="', '<use', 'filter', '<pattern', '&amp;', '&lt;']) if (svg.includes(k)) inc(misc, 'contains ' + k);
}
const sevs = ['S1','S2','S3','S4','S5'];
for (const e of cat.entries) {
  const variants = e.variants.length ? e.variants : [null];
  for (const theme of ['dark','light'])
    for (const v of variants) {
      scan(pictogramSvg(e, theme, v));
      for (const rep of ['standard','roados'])
        for (const sev of sevs) {
          scan(composeIcon({ entry: e, theme, representation: rep, variant: v, severity: sev }).svg);
          scan(composeIcon({ entry: e, theme, representation: rep, variant: v, severity: sev, distanceM: 1250, count: 3, sizeVariant: 'XL', direction: 'both', speed: { kind: 'posted', valueKmh: 100, unit: 'mph' } }).svg);
          scan(composeIcon({ entry: e, theme, representation: rep, variant: v, severity: sev, lane: {count:3,index:1}, speed: { kind: 'advisory', valueKmh: 20, unit: 'kmh' } }).svg);
          scan(composeIcon({ entry: e, theme, representation: rep, variant: v, severity: sev, speed: { kind: 'unknown', valueKmh: null, unit: 'kmh' } }).svg);
        }
    }
}
const dump = (t: string, m: Map<string, number>) => console.log(t, JSON.stringify(Object.fromEntries([...m].sort((a, b) => b[1] - a[1]))));
console.log('glyphs', Object.keys(GLYPHS).length, 'entries', cat.entries.length, 'issues', cat.issues.length, 'svgs', n, 'avgBytes', Math.round(bytes/n), 'maxBytes', maxB);
console.log('entries with variants', cat.entries.filter((e:any)=>e.variants.length).length, 'total variant combos', cat.entries.reduce((s:number,e:any)=>s+Math.max(1,e.variants.length),0));
console.log('with standardFrame', cat.entries.filter((e:any)=>e.standardFrame).length, 'slash', cat.entries.filter((e:any)=>e.visual.slash).length, 'legend', cat.entries.filter((e:any)=>e.visual.legend).length);
dump('elements', els); dump('attrs', attrs); dump('transforms', xf); dump('pathcmds', cmds); dump('misc', misc);
console.log('fonts', [...fonts]); console.log('baselines', [...baselines]); console.log('texts', [...texts].join(' | '));
const frames: Record<string, number> = {}; for (const e of cat.entries) frames[e.frame] = (frames[e.frame] ?? 0) + 1; console.log('frames', JSON.stringify(frames));
const pot = cat.entries.filter((e:any) => e.visual.glyph === 'pothole').map((e:any) => `${e.id}:${e.slug}`); console.log('pothole glyph entries', pot.join(', '));
