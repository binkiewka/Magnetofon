#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR="$ROOT/vendor/projectm"
BUNDLE="$ROOT/resources/projectm/linux-x64"
BUILD="$VENDOR/frontend-build-magnetofon"

mkdir -p "$VENDOR" "$BUNDLE/bin" "$BUNDLE/presets" "$BUNDLE/textures"

missing=()
for cmd in git cmake rsync pkg-config; do
  command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done
pkg-config --exists sdl2 2>/dev/null || missing+=("pkg-config:sdl2")
if ((${#missing[@]})); then
  cat >&2 <<MSG
Missing projectM build dependencies: ${missing[*]}

On Ubuntu/Debian, install roughly:
  sudo apt install git cmake build-essential pkg-config rsync libsdl2-dev

Poco is intentionally built into the Magnetofon projectM bundle because Ubuntu 24.04's
libpoco-dev is 1.11.0, which projectMSDL rejects due to a known crash bug.
MSG
  exit 1
fi

if [[ ! -d "$VENDOR/frontend-sdl-cpp/.git" ]]; then
  git clone --depth 1 --recurse-submodules https://github.com/projectM-visualizer/frontend-sdl-cpp.git "$VENDOR/frontend-sdl-cpp"
fi

if [[ ! -d "$VENDOR/projectm/.git" ]]; then
  git clone --depth 1 --recurse-submodules https://github.com/projectM-visualizer/projectm.git "$VENDOR/projectm"
fi

if [[ ! -d "$VENDOR/poco/.git" ]]; then
  git clone --depth 1 --branch poco-1.14.2-release https://github.com/pocoproject/poco.git "$VENDOR/poco"
fi

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

echo "Bundled projectM helper: $BUNDLE/bin/projectMSDL"
