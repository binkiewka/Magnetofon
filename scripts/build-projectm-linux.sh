#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR="$ROOT/vendor/projectm"
BUNDLE="$ROOT/resources/projectm/linux-x64"
BUILD="$VENDOR/frontend-build-magnetofon"

mkdir -p "$VENDOR" "$BUNDLE/bin" "$BUNDLE/presets" "$BUNDLE/textures"

missing=()
for cmd in git cmake rsync pkg-config sha256sum python3; do
  command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done
pkg-config --exists sdl2 2>/dev/null || missing+=("pkg-config:sdl2")
pkg-config --exists libpulse 2>/dev/null || missing+=("pkg-config:libpulse")
if ((${#missing[@]})); then
  cat >&2 <<MSG
Missing projectM build dependencies: ${missing[*]}

On Ubuntu/Debian, install roughly:
  sudo apt install git cmake build-essential pkg-config rsync libsdl2-dev libpulse-dev

Poco is intentionally built into the Magnetofon projectM bundle because Ubuntu 24.04's
libpoco-dev is 1.11.0, which projectMSDL rejects due to a known crash bug.
MSG
  exit 1
fi

# These are the source revisions used by the existing patched bundle, not
# moving default-branch HEADs. Never reset an existing developer checkout.
FRONTEND_COMMIT=49863dd3343f8fe926c0d50db2436aa52db2aec6
CORE_COMMIT=359bf7801e20b87712d6a89c8862f770c84ecdeb
POCO_COMMIT=96d182a99303fb068575294b36f0cc20da2e7b25

checkout_pinned() {
  local repo="$1" url="$2" commit="$3"
  if [[ ! -d "$repo/.git" ]]; then
    git init "$repo"
    git -C "$repo" remote add origin "$url"
    git -C "$repo" fetch --depth 1 origin "$commit"
    git -C "$repo" checkout --detach "$commit"
  fi
  if [[ "$(git -C "$repo" rev-parse HEAD)" != "$commit" ]]; then
    echo "Source revision mismatch in $repo; expected $commit. Preserve your changes and use a separate pinned checkout." >&2
    exit 1
  fi
}

checkout_pinned "$VENDOR/frontend-sdl-cpp" https://github.com/projectM-visualizer/frontend-sdl-cpp.git "$FRONTEND_COMMIT"
checkout_pinned "$VENDOR/projectm" https://github.com/projectM-visualizer/projectm.git "$CORE_COMMIT"
checkout_pinned "$VENDOR/poco" https://github.com/pocoproject/poco.git "$POCO_COMMIT"

(cd "$ROOT/resources/projectm/patches" && sha256sum --check SHA256SUMS)

git -C "$VENDOR/frontend-sdl-cpp" submodule update --init --recursive
git -C "$VENDOR/projectm" submodule update --init --recursive

apply_patch_once() {
  local repo="$1"
  local patch_file="$2"

  if git -C "$repo" apply --check "$patch_file"; then
    git -C "$repo" apply "$patch_file"
  elif git -C "$repo" apply --reverse --check "$patch_file"; then
    echo "Already applied: $patch_file"
  else
    echo "Patch no longer applies cleanly: $patch_file" >&2
    exit 1
  fi
}

apply_patch_once "$VENDOR/projectm" "$ROOT/resources/projectm/patches/projectm-core.patch"
apply_patch_once "$VENDOR/frontend-sdl-cpp" "$ROOT/resources/projectm/patches/frontend-sdl-cpp.patch"
apply_patch_once "$VENDOR/frontend-sdl-cpp" "$ROOT/resources/projectm/patches/window-drag.patch"
apply_patch_once "$VENDOR/frontend-sdl-cpp" "$ROOT/resources/projectm/patches/magnetofon-audio.patch"

POCO_BUILD="$VENDOR/poco-build-magnetofon"
cmake -S "$VENDOR/poco" -B "$POCO_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$BUNDLE" \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_TESTS=OFF \
  -DENABLE_SAMPLES=OFF \
  -DENABLE_ENCODINGS=OFF \
  -DENABLE_ENCODINGS_COMPILER=OFF \
  -DENABLE_XML=ON \
  -DENABLE_JSON=ON \
  -DENABLE_UTIL=ON \
  -DENABLE_NET=OFF \
  -DENABLE_CRYPTO=OFF \
  -DENABLE_NETSSL=OFF \
  -DENABLE_DATA=OFF \
  -DENABLE_MONGODB=OFF \
  -DENABLE_REDIS=OFF \
  -DENABLE_PROMETHEUS=OFF \
  -DENABLE_ACTIVERECORD=OFF \
  -DENABLE_ACTIVERECORD_COMPILER=OFF
cmake --build "$POCO_BUILD" --config Release --parallel "$(nproc)"
cmake --install "$POCO_BUILD"

CORE_BUILD="$VENDOR/core-build-magnetofon"
cmake -S "$VENDOR/projectm" -B "$CORE_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$BUNDLE" \
  -DCMAKE_INSTALL_RPATH='$ORIGIN'
cmake --build "$CORE_BUILD" --config Release --parallel "$(nproc)"
cmake --install "$CORE_BUILD"

cmake -S "$VENDOR/frontend-sdl-cpp" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$BUNDLE" \
  -DPoco_DIR="$BUNDLE/lib/cmake/Poco" \
  -DPoco_VERSION=1.14.2 \
  -DMAGNETOFON_AUDIO_SOURCE_DIR="$ROOT/src-cpp/audio" \
  -DCMAKE_INSTALL_PREFIX="$BUNDLE" \
  -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib' \
  -DDEFAULT_CONFIG_PATH='${application.dir}/../share/projectMSDL' \
  -DDEFAULT_PRESETS_PATH='${application.dir}/../presets' \
  -DDEFAULT_TEXTURES_PATH='${application.dir}/../textures'
cmake --build "$BUILD" --config Release --parallel "$(nproc)"
cmake --install "$BUILD"

if [[ ! -x "$BUNDLE/bin/projectMSDL" ]]; then
  echo "projectMSDL was not installed to $BUNDLE/bin/projectMSDL" >&2
  exit 1
fi

# Copy non-system shared-library dependencies next to the helper. Magnetofon sets LD_LIBRARY_PATH
# to this folder when launching projectM, so the packaged app uses its bundled projectM runtime.
while IFS= read -r dep; do
  [[ -f "$dep" ]] || continue
  base="$(basename "$dep")"
  case "$base" in
    libprojectM-*|libPoco*)
      target="$BUNDLE/lib/$base"
      if [[ "$(readlink -f "$dep")" != "$(readlink -f "$target" 2>/dev/null || true)" ]]; then
        cp -L "$dep" "$BUNDLE/lib/"
      fi
      ;;
  esac
done < <(ldd "$BUNDLE/bin/projectMSDL" 2>/dev/null | awk '/=> \// { print $3 }')

# Seed with upstream presets/textures when present; Tom's giant local library stays under visuals/.
rsync -a --include='*/' --include='*.milk' --exclude='*' "$VENDOR/projectm/presets/" "$BUNDLE/presets/" 2>/dev/null || true
rsync -a "$VENDOR/projectm/textures/" "$BUNDLE/textures/" 2>/dev/null || true

# Keep a portable source/binary receipt with the bundled helper.
python3 - "$ROOT" "$BUNDLE" "$FRONTEND_COMMIT" "$CORE_COMMIT" "$POCO_COMMIT" <<'PYINFO'
import hashlib,json,pathlib,sys
root,bundle=map(pathlib.Path,sys.argv[1:3])
files=["src-cpp/audio/PulseAudioAnalyzer.cpp","src-cpp/audio/PulseAudioAnalyzer.hpp"]
files += [str(p.relative_to(root)) for p in sorted((root/"resources/projectm/patches").glob("*.patch"))]
receipt={"format":1,"frontendCommit":sys.argv[3],"coreCommit":sys.argv[4],"pocoCommit":sys.argv[5],
         "features":["process-isolated-pcm","audio-readiness-heartbeat","F-F11-Escape-fullscreen"],
         "sourceSha256":{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in files},
         "helperSha256":hashlib.sha256((bundle/"bin/projectMSDL").read_bytes()).hexdigest()}
(bundle/"magnetofon-runtime.json").write_text(json.dumps(receipt,indent=2)+"\n")
PYINFO

python3 "$ROOT/scripts/verify-projectm-runtime.py" --write

echo "Bundled projectM helper: $BUNDLE/bin/projectMSDL"
