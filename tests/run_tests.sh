#!/usr/bin/env bash
# Builds and runs the host-side tests for the pure-C++ core modules.
# Optionally verifies against a real ROM:
#   ./tests/run_tests.sh "$HOME/Downloads/roms/n64/Super Mario 64 (USA).z64"
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/parallel_launcher_tests"

clang++ -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter \
    -I"$ROOT" \
    "$ROOT/tests/test_core.cpp" \
    "$ROOT/src/core/checksum.cpp" \
    "$ROOT/src/core/rom.cpp" \
    "$ROOT/src/core/patch.cpp" \
    "$ROOT/src/core/text_util.cpp" \
    -o "$OUT"

"$OUT" "$@"
