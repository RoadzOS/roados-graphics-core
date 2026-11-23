#!/usr/bin/env bash
# Fetch and build the pinned Skia revision used by RoadOS Graphics Core.
#
# Builds Skia FROM SOURCE using Skia's own GN + Ninja toolchain. No prebuilt
# Skia binaries are downloaded. The only downloaded executables are `gn` and
# `ninja`, fetched by Skia's own bin/fetch-gn and bin/fetch-ninja scripts from
# Google's official CIPD server (chrome-infra-packages.appspot.com), pinned by
# revision (ninja is additionally SHA-256 verified by Skia's script).
#
# Only the third-party dependencies that our configuration needs are fetched
# (expat, libpng, zlib), each at the exact commit pinned in Skia's DEPS file.
#
# Usage: scripts/bootstrap_skia.sh [Release|Debug] [TARGET]
#
# TARGET (default: host):
#   host            the machine running the script (macOS or Linux)
#   ios-arm64       iOS devices                       (needs Xcode)
#   iossim-arm64    iOS Simulator on Apple silicon     (needs Xcode)
#   android-arm64   Android arm64-v8a                  (needs ANDROID_NDK_HOME)
#   android-x64     Android x86_64 (emulator)          (needs ANDROID_NDK_HOME)
#
# Output: third_party/skia/out/<Config>-<os>-<cpu>, e.g. Release-iossim-arm64.
# The source checkout is shared, so each extra target costs ~25 MB.
#
# Environment: ROADOS_SKIA_DIR (shared checkout), ROADOS_IOS_MIN (default 16.0),
#              ROADOS_ANDROID_API (default 24), ANDROID_NDK_HOME.
set -euo pipefail

SKIA_COMMIT="37e919d8669ebe4a8912dcef03db14b9b55f70c7"   # chrome/m158, 2026-11-20
SKIA_BRANCH="chrome/m158"
SKIA_URL="https://skia.googlesource.com/skia"
NEEDED_DEPS=(expat libpng zlib)

CONFIG="${1:-Release}"
TARGET="${2:-host}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKIA_DIR="${ROADOS_SKIA_DIR:-$ROOT/third_party/skia}"

case "$(uname -s)" in
  Darwin) HOST_OS=mac ;;
  Linux)  HOST_OS=linux ;;
  *) echo "Unsupported host OS for this script: $(uname -s)" >&2; exit 1 ;;
esac
HOST_CPU="$(uname -m)"; [[ "$HOST_CPU" == "aarch64" ]] && HOST_CPU=arm64; [[ "$HOST_CPU" == "x86_64" ]] && HOST_CPU=x64

# Target platform → GN arguments and output directory name.
TARGET_ARGS=""
case "$TARGET" in
  host)
    OUT_OS="$HOST_OS"; OUT_CPU="$HOST_CPU" ;;
  ios-arm64|iossim-arm64)
    [[ "$HOST_OS" == mac ]] || { echo "iOS targets need a macOS host" >&2; exit 1; }
    OUT_CPU=arm64
    if [[ "$TARGET" == ios-arm64 ]]; then OUT_OS=ios; SIM=false; else OUT_OS=iossim; SIM=true; fi
    TARGET_ARGS="target_os=\"ios\" target_cpu=\"arm64\" ios_use_simulator=$SIM ios_min_target=\"${ROADOS_IOS_MIN:-16.0}\""
    ;;
  android-arm64|android-x64)
    NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
    [[ -d "$NDK" ]] || { echo "Set ANDROID_NDK_HOME to an Android NDK (r26 or newer)" >&2; exit 1; }
    OUT_OS=android; OUT_CPU="${TARGET#android-}"
    NEEDED_DEPS+=(freetype)
    TARGET_ARGS="target_os=\"android\" target_cpu=\"$OUT_CPU\" ndk=\"$NDK\" ndk_api=${ROADOS_ANDROID_API:-24}
skia_use_freetype=true skia_use_system_freetype2=false skia_enable_fontmgr_android=true
skia_enable_fontmgr_android_ndk=false"
    ;;
  *) echo "Unknown target: $TARGET" >&2; exit 1 ;;
esac
OUT="out/${CONFIG}-${OUT_OS}-${OUT_CPU}"

log() { printf '\033[1;34m[skia]\033[0m %s\n' "$*"; }

fetch_at() { # dir url sha
  local dir="$1" url="$2" sha="$3"
  if [[ -d "$dir/.git" ]] && [[ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" == "$sha" ]]; then
    log "ok   $(basename "$dir") @ ${sha:0:12}"; return
  fi
  log "get  $(basename "$dir") @ ${sha:0:12}"
  rm -rf "$dir"; mkdir -p "$dir"
  git -C "$dir" init -q
  git -C "$dir" remote add origin "$url"
  git -C "$dir" fetch -q --depth 1 origin "$sha"
  git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
}

# 1. Skia itself.
fetch_at "$SKIA_DIR" "$SKIA_URL" "$SKIA_COMMIT"

# 2. Only the DEPS we need, at Skia's pinned commits.
for dep in "${NEEDED_DEPS[@]}"; do
  line="$(grep -E "\"third_party/externals/${dep}\"" "$SKIA_DIR/DEPS")" || { echo "DEPS has no $dep" >&2; exit 1; }
  spec="$(sed -E 's/.*: *"([^"]+)".*/\1/' <<<"$line")"
  fetch_at "$SKIA_DIR/third_party/externals/$dep" "${spec%@*}" "${spec##*@}"
done

# 3. Official build tools.
( cd "$SKIA_DIR" && [[ -x bin/gn ]] || python3 bin/fetch-gn )
( cd "$SKIA_DIR" && python3 bin/fetch-ninja )
NINJA="$SKIA_DIR/third_party/ninja/ninja"

# 4. Configure. CPU raster backend, SVG module, PNG codec, platform font host.
IS_DEBUG=false; [[ "$CONFIG" == "Debug" ]] && IS_DEBUG=true
ARGS="
is_official_build=true
is_debug=$IS_DEBUG
is_component_build=false
skia_enable_tools=false
skia_enable_svg=true
skia_use_expat=true
skia_use_system_expat=false
skia_use_libpng_decode=true
skia_use_libpng_encode=true
skia_use_system_libpng=false
skia_use_zlib=true
skia_use_system_zlib=false
skia_use_libjpeg_turbo_decode=false
skia_use_libjpeg_turbo_encode=false
skia_use_libwebp_decode=false
skia_use_libwebp_encode=false
skia_use_wuffs=false
skia_use_dng_sdk=false
skia_use_piex=false
skia_use_harfbuzz=false
skia_use_icu=false
skia_enable_pdf=false
skia_enable_skottie=false
skia_enable_ganesh=false
skia_enable_graphite=false
skia_use_gl=false
skia_use_metal=false
skia_use_vulkan=false
skia_use_xps=false
skia_use_partition_alloc=false
"
ARGS+="$TARGET_ARGS
"
if [[ "$TARGET" == host && "$HOST_OS" == "linux" ]]; then
  ARGS+="skia_use_freetype=true
skia_use_system_freetype2=true
skia_use_fontconfig=true
"
fi
ARGS="$(tr '\n' ' ' <<<"$ARGS")"

log "gn gen $OUT"
( cd "$SKIA_DIR" && bin/gn gen "$OUT" --args="$ARGS" )

# 5. Build only the targets we link against.
log "ninja $OUT"
"$NINJA" -C "$SKIA_DIR/$OUT" skia svg skshaper skresources

log "done: $SKIA_DIR/$OUT"
ls -1 "$SKIA_DIR/$OUT"/*.a
