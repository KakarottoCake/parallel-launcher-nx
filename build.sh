#!/usr/bin/env bash
# Configure + build the Switch NRO. Pass "clean" to wipe the build directory.
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$PATH"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build_switch"

if [[ "${1:-}" == "clean" ]]; then
    rm -rf "$BUILD"
fi

cmake -S "$ROOT" -B "$BUILD" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM_SWITCH=ON \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"

cmake --build "$BUILD" "$@"

echo
echo "NRO: $BUILD/parallel_launcher.nro"
ls -la "$BUILD/parallel_launcher.nro"
