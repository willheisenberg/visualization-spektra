#!/usr/bin/env bash
# Läuft im Build-Container: Kerneinheiten für amd64 bauen und testen.
set -euo pipefail

src=/src/visualization.spektra
out=$src/build/amd64-tests

cmake -S "$src/tests/cpp" -B "$out" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$out" -j"$(nproc)"
ctest --test-dir "$out" --output-on-failure -j"$(nproc)"
