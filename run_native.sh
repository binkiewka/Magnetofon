#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$DIR/src-cpp/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f "MagnetofonNative" ]; then
    echo "[Magnetofon] Building native C++ application..."
    cmake ..
    make -j$(nproc)
fi

echo "[Magnetofon] Launching native C++ Hi-Fi audio player..."
export LC_NUMERIC=C
export QSG_RHI_BACKEND=opengl
exec ./MagnetofonNative "$@"
