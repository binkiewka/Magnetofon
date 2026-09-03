#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$DIR/src-cpp/build"

echo "[Magnetofon] Updating native C++ build..."
cmake -S "$DIR/src-cpp" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "[Magnetofon] Launching native C++ Hi-Fi audio player..."
export LC_NUMERIC=C
export QSG_RHI_BACKEND=opengl
exec "$BUILD_DIR/MagnetofonNative" "$@"
