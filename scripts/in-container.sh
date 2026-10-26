#!/usr/bin/env bash
# Führt einen Befehl im Build-Container aus.
# Das Projekt liegt dort unter /src/visualization.spektra (Arbeitsverzeichnis).
set -euo pipefail

if [[ $# -eq 0 ]]; then
  echo "Aufruf: scripts/in-container.sh <befehl> [argumente…]" >&2
  exit 2
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
image="spektra-build:21.3-omega"

docker build -q -t "$image" "$root/docker" >/dev/null
# --init: Ctrl-C erreicht make/cmake/ctest, statt vom PID-1-Prozess ignoriert zu werden.
docker run --rm --init \
  -u "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$root":/src/visualization.spektra \
  -w /src/visualization.spektra \
  "$image" "$@"
