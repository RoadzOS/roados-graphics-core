#!/usr/bin/env bash
# Regenerates assets/rvhl/ from the pinned Studio submodule.
# Requires Node 18+ (only maintainers updating the bundle need this; C++
# consumers use the committed bundle).
#
#   scripts/export_rvhl.sh            regenerate assets/rvhl
#   scripts/export_rvhl.sh --check    fail if the committed bundle differs (CI)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STUDIO="$ROOT/external/roados-visual-hazard-studio"
OUT="$ROOT/assets/rvhl"

[[ -f "$STUDIO/package.json" ]] || { echo "Studio submodule missing: git submodule update --init" >&2; exit 1; }
[[ -d "$STUDIO/node_modules" ]] || (cd "$STUDIO" && npm ci --silent)

if [[ "${1:-}" == "--check" ]]; then
  TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
  (cd "$STUDIO" && npx --no-install tsx "$ROOT/tools/rvhl-export/export.mts" "$STUDIO" "$TMP/rvhl")
  diff -r "$OUT" "$TMP/rvhl" >/dev/null || { echo "assets/rvhl is out of date: run scripts/export_rvhl.sh" >&2; exit 1; }
  echo "assets/rvhl is up to date."
else
  (cd "$STUDIO" && npx --no-install tsx "$ROOT/tools/rvhl-export/export.mts" "$STUDIO" "$OUT")
fi
