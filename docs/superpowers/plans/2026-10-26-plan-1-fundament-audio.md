# Spektra Plan 1: Fundament und Audio-Analyse – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein installierbares Kodi-Add-on `visualization.spektra`, das Kodis Audiodaten exakt wie der Prototyp analysiert (vier Bänder, Anschläge, Tonhöhe, Intensität, Drop, Feinspektrum), alle Einstellungen mit Standardwerten kennt, `live.json` übernimmt, `renderer.json` als Herzschlag schreibt und zur Sichtprüfung vier Diagnose-Ringe zeichnet, die mit den Bändern pulsieren.

**Architecture:** Kodi- und GL-freie Kerneinheiten in `src/core/` (FFT, Spektrum, Analyzer, Settings, Routing, StateFiles, SampleRing, Engine) werden auf amd64 im Build-Container mit GoogleTest geprüft. `src/addon.cpp` hängt die prozessweite `Engine` an Kodi; `src/gl/DiagnosticRenderer` zeichnet vier Ringe mit GLES 3. Build, Container und Prüfskripte stammen aus Party Video.

**Tech Stack:** C++17, OpenGL ES 3.0, Kodi 21.3 Omega dev-kit (`cmake/addons`), GoogleTest, nlohmann/json, Docker (Debian Bookworm, aarch64-Cross-Toolchain).

**Spec:** `docs/superpowers/specs/2026-10-26-spektra-design.md`

Folgepläne: Plan 2 „Szene Eclipse und GL-Renderer“, Plan 3 „Service, Einstellungsdialog-Anbindung und JSON-RPC“.

## Global Constraints

- Add-on-ID `visualization.spektra`, Name „Spektra“, Version `0.1.0`; die Version steht nur in `visualization.spektra/addon.xml.in`.
- Ziel: Raspberry Pi 5 (aarch64), LibreELEC 12, Kodi 21 „Omega“; erlaubte Laufzeitbibliotheken: `libGLESv2.so.2 libstdc++.so.6 libm.so.6 libgcc_s.so.1 libc.so.6`; GLIBC ≤ 2.38, GLIBCXX ≤ 3.4.32.
- C++17, OpenGL ES 3.0 (`#version 300 es`). Kein FFmpeg.
- Kodi liefert Audio als Float, interleaved, Stereo, 44 100 Hz (`ActiveAE`: `vizFormat` 2.0, 44100).
- Analyse wie Prototyp `docs/entwuerfe/dune-v14-favorit.html`: FFT 4096, Blackman-Fenster, Glättung 0,5, dB-Bereich −100…−30, Byte-Quantisierung, lineare Rückrechnung.
- Logische Bezugsgröße 1280×720.
- **Nicht committen.** Persönliche Regel des Maintainers: Commits und Pushes nur auf ausdrückliche Anweisung. Jede Aufgabe endet mit „Stand prüfen“ statt Commit; Änderungen bleiben im Arbeitsverzeichnis.
- Pi `root@192.168.178.10`: nur lesend (Logs, `addon_data`). Kein scp, keine Installation, kein Neustart.

## Review Focus

1. Musik pausiert oder stoppt → alle Pegel fallen innerhalb von ~3 s auf ≈ 0, statt auf dem letzten Wert einzufrieren (Task 7: `EngineTest.SilenceAfterMusicDecays`).
2. Songwechsel → Kodi erzeugt eine neue Instanz; Analysezustand und Einstellungen bleiben erhalten (Task 7: `EngineTest.StartKeepsAnalysisState`).
3. Mono- oder Mehrkanal-Audio (`channels` ≠ 2) → korrekt zu Mono gemischt, keine verschobenen Samples (Task 6: `SampleRingTest.DownmixesAnyChannelCount`).
4. Kaputte oder falsch typisierte `live.json` → bisherige Werte bleiben, nichts stürzt ab (Task 5: `SettingsTest.LiveJsonGarbageIsIgnored`).
5. Lücke im Audio (> 1 s, z. B. Spulen) → Langzeitmittel neu einpegeln, statt Sekunden lang falsche Pegel zu zeigen (Task 7: `EngineTest.GapTriggersRebase`).

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `CLAUDE.md` | Projektregeln (Befehle, Pi nur lesend, kein Commit) |
| `.gitignore`, `build.sh`, `test.sh` | Gerüst |
| `docker/Dockerfile`, `docker/aarch64-toolchain.cmake` | Build-Container |
| `scripts/in-container.sh`, `scripts/build-in-container.sh`, `scripts/test-cpp-in-container.sh`, `scripts/check_elf.sh`, `scripts/check_zip.sh` | Build und Prüfungen |
| `tests/build/test_check_elf.sh`, `tests/build/test_check_zip.sh` | Tests der Prüfskripte |
| `CMakeLists.txt` | Add-on-Build über Kodis `build_addon` |
| `visualization.spektra/addon.xml.in` | Add-on-Manifest |
| `visualization.spektra/resources/settings.xml`, `resources/language/resource.language.de_de/strings.po`, `resource.language.en_gb/strings.po` | Einstellungen + Texte |
| `third_party/nlohmann/json.hpp` | JSON (aus Party Video kopiert) |
| `src/core/Fft.{h,cpp}` | reelle FFT, Beträge |
| `src/core/Spectrum.{h,cpp}` | WebAudio-Analyser-Nachbau: Fenster, Glättung, dB, Byte, linear |
| `src/core/AudioAnalyzer.{h,cpp}` | Bänder, Pegel, Anschläge, Tonhöhe, Intensität, Drop, Feinspektrum |
| `src/core/Settings.{h,cpp}` | Einstellungswerte, Standards, Schlüssel, Typen, Anwenden aus Text |
| `src/core/Routing.{h,cpp}` | Effekt-Pegel/-Anschlag aus Zuordnung |
| `src/core/StateFiles.{h,cpp}` | `live.json` lesen, `renderer.json` schreiben |
| `src/core/SampleRing.{h,cpp}` | thread-sicherer Mono-Ringpuffer |
| `src/core/Engine.{h,cpp}` | prozessweite Verbindung von allem, Zeitsteuerung |
| `src/gl/DiagnosticRenderer.{h,cpp}` | vier Ringe (GLES 3) |
| `src/addon.{h,cpp}` | Kodi-Instanz |
| `tests/cpp/CMakeLists.txt`, `tests/cpp/*_test.cpp`, `tests/cpp/TempDir.h` | amd64-Tests |

---

### Task 1: Repo-Gerüst, Build-Container, Prüfskripte und baubares Leer-Add-on

**Files:**
- Create: `CLAUDE.md`, `build.sh`, `test.sh`, `docker/Dockerfile`, `docker/aarch64-toolchain.cmake`, `scripts/in-container.sh`, `scripts/build-in-container.sh`, `scripts/check_elf.sh`, `scripts/check_zip.sh`, `tests/build/test_check_elf.sh`, `tests/build/test_check_zip.sh`, `CMakeLists.txt`, `visualization.spektra/addon.xml.in`, `src/addon.h`, `src/addon.cpp`, `third_party/nlohmann/json.hpp`
- Modify: `.gitignore`

**Interfaces:**
- Produces: `./build.sh` → `dist/visualization.spektra-0.1.0.zip`; `scripts/in-container.sh <cmd>` führt im Image `spektra-build:21.3-omega` mit Projekt unter `/src/visualization.spektra` aus.

- [ ] **Step 1: Projektregeln und Ignore-Liste**

`CLAUDE.md`:

```markdown
# CLAUDE.md

## Befehle

    ./build.sh    # Addon-Zip bauen (Docker) → dist/
    ./test.sh     # Prüfskripte, C++-Tests, Build

## Regeln

- Nie committen oder pushen ohne ausdrückliche Anweisung.
- Pi `root@192.168.178.10`: nur lesender Zugriff (Logs, addon_data). Kein scp, keine
  Installation, kein Neustart – das macht der Maintainer.
- Die Addon-Version steht nur in `visualization.spektra/addon.xml.in`.
- Kerneinheiten in `src/core/` und Szenen in `src/scene/` kennen weder Kodi noch GL.
- Referenz für Aussehen und Parameter: `docs/entwuerfe/dune-v14-favorit.html`.
```

`.gitignore` (ersetzt die bestehende Datei, `.superpowers/` bleibt drin):

```
/build/
/dist/
/.venv/
/.superpowers/
__pycache__/
.ruff_cache/
.pytest_cache/
```

- [ ] **Step 2: Build-Container**

`docker/Dockerfile`:

```dockerfile
FROM debian:bookworm

ARG KODI_TAG=21.3-Omega

RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential crossbuild-essential-arm64 cmake git ca-certificates \
      pkg-config zip unzip file \
      libgles-dev:arm64 libgtest-dev \
 && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch "${KODI_TAG}" https://github.com/xbmc/xbmc.git /opt/kodi

COPY aarch64-toolchain.cmake /opt/toolchains/aarch64.cmake
```

`docker/aarch64-toolchain.cmake`:

```cmake
# Cross-Build für Raspberry Pi 5 (LibreELEC 12, aarch64) auf Debian Bookworm.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Multiarch-Pakete (libgles-dev:arm64) liegen unter /usr/lib/aarch64-linux-gnu.
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)

# Programme vom Host, Bibliotheken/Header/Pakete aus Root-Pfad und Host-Präfixen:
# Kodis cmake/addons legt KodiConfig.cmake in einem Host-Pfad des Build-Verzeichnisses ab.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
```

`scripts/in-container.sh`:

```bash
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
```

- [ ] **Step 3: Prüfskripte mit ihren Tests zuerst**

`tests/build/test_check_elf.sh`:

```bash
#!/usr/bin/env bash
# Läuft im Build-Container. Prüft scripts/check_elf.sh mit kleinen Fixture-Bibliotheken.
set -euo pipefail
cd "$(dirname "$0")/../.."

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

printf '#include <stdio.h>\nint f(void) { return puts("x"); }\n' > "$tmp/f.c"
aarch64-linux-gnu-gcc -shared -fPIC "$tmp/f.c" -o "$tmp/good.so"
aarch64-linux-gnu-gcc -shared -fPIC "$tmp/f.c" -Wl,--no-as-needed -lresolv -o "$tmp/badlib.so"
gcc -shared -fPIC "$tmp/f.c" -o "$tmp/x86.so"

passed=0
failed=0
expect() { # expect <ok|fail> <beschreibung> <befehl …>
  local want="$1" desc="$2" got
  shift 2
  if "$@" >/dev/null 2>&1; then got=ok; else got=fail; fi
  if [[ "$got" == "$want" ]]; then
    echo "PASS $desc"
    passed=$((passed + 1))
  else
    echo "FAIL $desc (erwartet $want, erhalten $got)"
    failed=$((failed + 1))
  fi
}

expect ok   "saubere aarch64-Bibliothek"  scripts/check_elf.sh "$tmp/good.so"
expect fail "unerlaubte Abhängigkeit"      scripts/check_elf.sh "$tmp/badlib.so"
expect fail "falsche Architektur"          scripts/check_elf.sh "$tmp/x86.so"
expect fail "GLIBC-Version über Grenze"    env MAX_GLIBC=2.10 scripts/check_elf.sh "$tmp/good.so"
expect fail "Datei fehlt"                  scripts/check_elf.sh "$tmp/gibt-es-nicht.so"

echo "$passed bestanden, $failed fehlgeschlagen"
[[ "$failed" -eq 0 ]]
```

`tests/build/test_check_zip.sh`:

```bash
#!/usr/bin/env bash
# Läuft im Build-Container. Prüft scripts/check_zip.sh mit kleinen Addon-Zips.
set -euo pipefail
cd "$(dirname "$0")/../.."
root="$PWD"
id=visualization.spektra

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

printf '#include <stdio.h>\nint f(void) { return puts("x"); }\n' > "$tmp/f.c"
aarch64-linux-gnu-gcc -shared -fPIC "$tmp/f.c" -o "$tmp/arm.so"
gcc -shared -fPIC "$tmp/f.c" -o "$tmp/x86.so"

make_zip() { # make_zip <name> <bibliothek|-> <library_linux-name> [extra: symlink|pycache|noxml]
  local name="$1" lib="$2" libname="$3" extra="${4:-}"
  local dir="$tmp/$name/$id"
  mkdir -p "$dir"
  if [[ "$extra" != noxml ]]; then
    printf '<addon id="%s"><extension point="xbmc.player.musicviz" library_linux="%s"/></addon>\n' \
      "$id" "$libname" > "$dir/addon.xml"
  fi
  if [[ "$lib" != - ]]; then
    if [[ "$extra" == symlink ]]; then
      cp "$lib" "$dir/real.so"
      ln -s real.so "$dir/$libname"
    else
      cp "$lib" "$dir/$libname"
    fi
  fi
  if [[ "$extra" == pycache ]]; then
    mkdir -p "$dir/__pycache__"
    echo x > "$dir/__pycache__/default.cpython-311.pyc"
  fi
  (cd "$tmp/$name" && zip -qry "$tmp/$name.zip" "$id")
  echo "$tmp/$name.zip"
}

passed=0
failed=0
expect() { # expect <ok|fail> <beschreibung> <befehl …>
  local want="$1" desc="$2" got
  shift 2
  if "$@" >/dev/null 2>&1; then got=ok; else got=fail; fi
  if [[ "$got" == "$want" ]]; then
    echo "PASS $desc"
    passed=$((passed + 1))
  else
    echo "FAIL $desc (erwartet $want, erhalten $got)"
    failed=$((failed + 1))
  fi
}

expect ok   "gültiges Zip"                "$root/scripts/check_zip.sh" "$(make_zip good "$tmp/arm.so" lib.so.0.1.0)"
expect fail "Bibliothek fehlt"            "$root/scripts/check_zip.sh" "$(make_zip missing - lib.so.0.1.0)"
expect fail "Bibliothek ist Symlink"      "$root/scripts/check_zip.sh" "$(make_zip link "$tmp/arm.so" lib.so.0.1.0 symlink)"
expect fail "falsche Architektur"         "$root/scripts/check_zip.sh" "$(make_zip x86 "$tmp/x86.so" lib.so.0.1.0)"
expect fail "Python-Cache im Zip"         "$root/scripts/check_zip.sh" "$(make_zip cache "$tmp/arm.so" lib.so.0.1.0 pycache)"
expect fail "addon.xml fehlt"             "$root/scripts/check_zip.sh" "$(make_zip noxml "$tmp/arm.so" lib.so.0.1.0 noxml)"
expect fail "Zip fehlt"                   "$root/scripts/check_zip.sh" "$tmp/gibt-es-nicht.zip"

echo "$passed bestanden, $failed fehlgeschlagen"
[[ "$failed" -eq 0 ]]
```

- [ ] **Step 4: Tests laufen lassen – müssen scheitern**

Run: `chmod +x scripts/*.sh tests/build/*.sh && scripts/in-container.sh tests/build/test_check_elf.sh`
Expected: FAIL (`scripts/check_elf.sh` fehlt → alle „ok“-Erwartungen scheitern, Exit ≠ 0)

- [ ] **Step 5: Prüfskripte schreiben**

`scripts/check_elf.sh`:

```bash
#!/usr/bin/env bash
# Prüft, ob eine gebaute Addon-Bibliothek auf LibreELEC 12 (RPi5, aarch64) laden kann.
# Grenzen per Umgebung überschreibbar: MAX_GLIBC (2.38), MAX_GLIBCXX (3.4.32).
set -euo pipefail

so="${1:?Aufruf: check_elf.sh <datei.so>}"
readelf="${READELF:-aarch64-linux-gnu-readelf}"
max_glibc="${MAX_GLIBC:-2.38}"
max_glibcxx="${MAX_GLIBCXX:-3.4.32}"
allowed=" libGLESv2.so.2 libstdc++.so.6 libm.so.6 libgcc_s.so.1 libc.so.6 "

if [[ ! -f "$so" ]]; then
  echo "Datei nicht gefunden: $so"
  exit 1
fi

fail=0

machine="$("$readelf" -h "$so" | sed -n 's/^ *Machine: *//p')"
if [[ "$machine" != "AArch64" ]]; then
  echo "falsche Architektur: $machine"
  fail=1
fi

while read -r lib; do
  [[ -z "$lib" ]] && continue
  if [[ "$allowed" != *" $lib "* ]]; then
    echo "unerlaubte Abhängigkeit: $lib"
    fail=1
  fi
done < <("$readelf" -d "$so" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')

version_le() { # version_le a b → wahr, wenn a <= b
  [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -1)" == "$1" ]]
}

highest() { # highest PRÄFIX → höchste referenzierte Version oder leer
  "$readelf" -V "$so" | { grep -o "${1}_[0-9][0-9.]*" || true; } | sed "s/^${1}_//" | sort -V | tail -1
}

glibc="$(highest GLIBC)"
if [[ -n "$glibc" ]] && ! version_le "$glibc" "$max_glibc"; then
  echo "GLIBC_$glibc verlangt, erlaubt bis $max_glibc"
  fail=1
fi

glibcxx="$(highest GLIBCXX)"
if [[ -n "$glibcxx" ]] && ! version_le "$glibcxx" "$max_glibcxx"; then
  echo "GLIBCXX_$glibcxx verlangt, erlaubt bis $max_glibcxx"
  fail=1
fi

exit "$fail"
```

`scripts/check_zip.sh`:

```bash
#!/usr/bin/env bash
# Prüft das ausgelieferte Addon-Zip: addon.xml vorhanden, library_linux zeigt auf eine echte Datei
# im Zip, diese besteht check_elf.sh, und es ist kein Python-Cache enthalten.
set -euo pipefail

zip="${1:?Aufruf: check_zip.sh <addon.zip>}"
id=visualization.spektra

if [[ ! -f "$zip" ]]; then
  echo "Zip nicht gefunden: $zip"
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
unzip -q "$zip" -d "$tmp"

xml="$tmp/$id/addon.xml"
if [[ ! -f "$xml" ]]; then
  echo "addon.xml fehlt im Zip"
  exit 1
fi

lib="$(grep -o -m1 'library_linux="[^"]*"' "$xml" | sed 's/^library_linux="//; s/"$//' || true)"
if [[ -z "$lib" ]]; then
  echo "library_linux fehlt in addon.xml"
  exit 1
fi

so="$tmp/$id/$lib"
if [[ -L "$so" || ! -f "$so" ]]; then
  echo "Bibliothek $lib fehlt im Zip oder ist ein Symlink"
  exit 1
fi

if [[ -n "$(find "$tmp" \( -name __pycache__ -o -name '*.pyc' \) -print -quit)" ]]; then
  echo "Python-Cache im Zip"
  exit 1
fi

"$(dirname "$0")/check_elf.sh" "$so"
```

- [ ] **Step 6: Prüfskript-Tests laufen lassen – müssen bestehen**

Run: `chmod +x scripts/*.sh && scripts/in-container.sh tests/build/test_check_elf.sh && scripts/in-container.sh tests/build/test_check_zip.sh`
Expected: `5 bestanden, 0 fehlgeschlagen` und `7 bestanden, 0 fehlgeschlagen`

- [ ] **Step 7: Leer-Add-on und Build**

`third_party/nlohmann/json.hpp`: kopieren.

```bash
mkdir -p third_party/nlohmann
cp ../visualization.partyvideo/third_party/nlohmann/json.hpp third_party/nlohmann/json.hpp
```

`visualization.spektra/addon.xml.in`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<addon id="visualization.spektra" version="0.1.0" name="Spektra" provider-name="tesla">
  <requires>@ADDON_DEPENDS@</requires>
  <extension point="xbmc.player.musicviz" library_@PLATFORM@="@LIBRARY_FILENAME@"/>
  <extension point="xbmc.addon.metadata">
    <platform>@PLATFORM@</platform>
    <license>GPL-2.0-or-later</license>
    <summary lang="de_DE">Audio-reaktive Szenen zur Musik</summary>
    <summary lang="en_GB">Audio-reactive scenes for your music</summary>
    <description lang="de_DE">Übersetzt Bass, Tiefen, Mitten und Höhen in lebendige Animationen. Erste Szene: Eclipse, eine digitale Sonnenfinsternis aus Pixelstaub.</description>
    <description lang="en_GB">Turns bass, lows, mids and highs into living animations. First scene: Eclipse, a digital solar eclipse made of pixel dust.</description>
  </extension>
</addon>
```

`src/addon.h`:

```cpp
#pragma once

#include <kodi/addon-instance/Visualization.h>

// Kodi-Visualisierung Spektra. Kodi erzeugt diese Instanz bei jedem Songwechsel neu.
class ATTR_DLL_LOCAL CSpektra : public kodi::addon::CAddonBase,
                                public kodi::addon::CInstanceVisualization
{
public:
  CSpektra() = default;
  ~CSpektra() override = default;

  void Render() override;
};
```

`src/addon.cpp`:

```cpp
#include "addon.h"

#include <GLES3/gl3.h>

void CSpektra::Render()
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

ADDONCREATOR(CSpektra)
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.18)
project(visualization.spektra CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Kodi REQUIRED)

find_path(GLES3_INCLUDE_DIR GLES3/gl3.h REQUIRED)
find_library(GLESV2_LIBRARY GLESv2 REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/..
                    ${GLES3_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/third_party)

set(SPEKTRA_SOURCES src/addon.cpp)

set(SPEKTRA_HEADERS src/addon.h)

set(DEPLIBS ${GLESV2_LIBRARY})

build_addon(visualization.spektra SPEKTRA DEPLIBS)

include(CPack)
```

`scripts/build-in-container.sh`:

```bash
#!/usr/bin/env bash
# Läuft im Build-Container (scripts/in-container.sh). Baut das Addon über Kodis
# cmake/addons, prüft die Bibliothek und legt das Zip in dist/ ab.
set -euo pipefail

id=visualization.spektra
src=/src/$id
out=$src/build/aarch64

rm -rf "$out"
mkdir -p "$out/defs/$id" "$src/dist"

# Addon-Definition für cmake/addons; die URL wird wegen ADDON_SRC_PREFIX nicht benutzt.
echo "$id file://$src main" > "$out/defs/$id/$id.txt"
echo "linux" > "$out/defs/$id/platforms.txt"

cmake -S /opt/kodi/cmake/addons -B "$out/cmake" \
  -DCORE_SOURCE_DIR=/opt/kodi \
  -DCORE_SYSTEM_NAME=linux \
  -DAPP_RENDER_SYSTEM=gles \
  -DADDONS_TO_BUILD="$id" \
  -DADDONS_DEFINITION_DIR="$out/defs" \
  -DADDON_SRC_PREFIX=/src \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$out/install" \
  -DPACKAGE_ZIP=ON \
  -DPACKAGE_DIR="$out/zips" \
  -DCMAKE_TOOLCHAIN_FILE=/opt/toolchains/aarch64.cmake

cmake --build "$out/cmake" -j"$(nproc)"
cmake --build "$out/cmake" --target package-addons

zip="$(find "$out/zips" -type f -name "$id-*.zip" -print -quit)"
if [[ -z "$zip" ]]; then
  echo "Kein Zip unter $out/zips gefunden" >&2
  exit 1
fi

# Geprüft wird die Bibliothek, die tatsächlich ausgeliefert wird (gestrippt, im Zip).
"$src/scripts/check_zip.sh" "$zip"

rm -f "$src/dist/$id-"*.zip
cp "$zip" "$src/dist/"
unzip -l "$src/dist/$(basename "$zip")"
```

`build.sh`:

```bash
#!/usr/bin/env bash
# Baut das Addon-Zip für Raspberry Pi 5 / LibreELEC 12 / Kodi 21 im Docker-Container.
set -euo pipefail
cd "$(dirname "$0")"

scripts/in-container.sh scripts/build-in-container.sh
```

`test.sh` (Plan 3 ergänzt Python-Prüfungen):

```bash
#!/usr/bin/env bash
# Alle Prüfungen: Build-Skript-Tests und C++-Tests im Build-Container, danach das echte Zip.
set -euo pipefail
cd "$(dirname "$0")"

scripts/in-container.sh tests/build/test_check_elf.sh
scripts/in-container.sh tests/build/test_check_zip.sh
scripts/in-container.sh scripts/test-cpp-in-container.sh

# Auch Kodi-/GLES-Anbindung und das tatsächlich ausgelieferte Zip prüfen.
./build.sh
```

- [ ] **Step 8: Bauen**

Run: `chmod +x build.sh test.sh scripts/*.sh && ./build.sh`
Expected: Exit 0; `unzip -l` listet `visualization.spektra/addon.xml` und `visualization.spektra/visualization.spektra.so.0.1.0`; `dist/visualization.spektra-0.1.0.zip` existiert.

- [ ] **Step 9: Stand prüfen (nicht committen)**

Run: `git status --short`
Expected: neue Dateien sichtbar, `build/` und `dist/` ignoriert.

---

### Task 2: C++-Testgerüst und FFT

**Files:**
- Create: `scripts/test-cpp-in-container.sh`, `tests/cpp/CMakeLists.txt`, `tests/cpp/TempDir.h`, `src/core/Fft.h`, `src/core/Fft.cpp`, `tests/cpp/fft_test.cpp`

**Interfaces:**
- Produces:

```cpp
namespace spektra {
class Fft {
public:
  explicit Fft(size_t size);                                  // Zweierpotenz >= 2, sonst std::invalid_argument
  size_t Size() const { return m_n; }
  void Magnitudes(const float* input, float* magnitudes);     // input: Size() Werte, magnitudes: Size()/2 Werte |X[k]|
};
}
```

- [ ] **Step 1: Testgerüst**

`scripts/test-cpp-in-container.sh`:

```bash
#!/usr/bin/env bash
# Läuft im Build-Container: Kerneinheiten für amd64 bauen und testen.
set -euo pipefail

src=/src/visualization.spektra
out=$src/build/amd64-tests

cmake -S "$src/tests/cpp" -B "$out" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$out" -j"$(nproc)"
ctest --test-dir "$out" --output-on-failure -j"$(nproc)"
```

`tests/cpp/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.18)
project(spektra_core_tests CXX)

# Eigenständiger amd64-Testbuild der Kodi- und GL-freien Einheiten aus src/core und src/scene.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Werror)

set(CORE_SOURCES
  ${REPO_ROOT}/src/core/Fft.cpp
)

add_library(spektra_core STATIC ${CORE_SOURCES})
target_include_directories(spektra_core PUBLIC ${REPO_ROOT}/src)
target_include_directories(spektra_core SYSTEM PUBLIC ${REPO_ROOT}/third_party)
target_link_libraries(spektra_core PUBLIC Threads::Threads)

set(TEST_SOURCES
  fft_test.cpp
)

add_executable(core_tests ${TEST_SOURCES})
target_link_libraries(core_tests PRIVATE spektra_core GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(core_tests DISCOVERY_TIMEOUT 30)
```

`tests/cpp/TempDir.h`:

```cpp
#pragma once

#include <filesystem>
#include <random>
#include <string>

// Temporäres Verzeichnis für einen Test; wird im Destruktor entfernt.
class TempDir
{
public:
  TempDir()
  {
    std::random_device rd;
    m_path = std::filesystem::temp_directory_path() / ("spektra-test-" + std::to_string(rd()));
    std::filesystem::create_directories(m_path);
  }
  ~TempDir() { std::filesystem::remove_all(m_path); }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& Path() const { return m_path; }
  std::string File(const std::string& name) const { return (m_path / name).string(); }

private:
  std::filesystem::path m_path;
};
```

- [ ] **Step 2: Failing test**

`tests/cpp/fft_test.cpp`:

```cpp
#include "core/Fft.h"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

using spektra::Fft;

TEST(FftTest, ImpulseHasFlatSpectrum)
{
  Fft fft(8);
  std::vector<float> in(8, 0.0f), mag(4);
  in[0] = 1.0f;
  fft.Magnitudes(in.data(), mag.data());
  for (float m : mag)
    EXPECT_NEAR(m, 1.0f, 1e-5f);
}

TEST(FftTest, CosineLandsInItsBin)
{
  const size_t n = 64;
  Fft fft(n);
  std::vector<float> in(n), mag(n / 2);
  for (size_t i = 0; i < n; ++i)
    in[i] = std::cos(2.0 * M_PI * 3.0 * i / n);
  fft.Magnitudes(in.data(), mag.data());
  EXPECT_NEAR(mag[3], 32.0f, 1e-3f);
  for (size_t k = 0; k < n / 2; ++k)
    if (k != 3)
      EXPECT_NEAR(mag[k], 0.0f, 1e-3f) << "Bin " << k;
}

TEST(FftTest, LargeSizeMatchesDirectDft)
{
  const size_t n = 4096;
  Fft fft(n);
  std::vector<float> in(n), mag(n / 2);
  for (size_t i = 0; i < n; ++i)
    in[i] = 0.3f * std::sin(2.0 * M_PI * 100.5 * i / n) + 0.1f * std::cos(2.0 * M_PI * 777.0 * i / n);
  fft.Magnitudes(in.data(), mag.data());
  for (size_t k : {0u, 100u, 101u, 777u, 2000u})
  {
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
      re += in[i] * std::cos(2.0 * M_PI * k * i / n);
      im -= in[i] * std::sin(2.0 * M_PI * k * i / n);
    }
    EXPECT_NEAR(mag[k], std::hypot(re, im), 0.05) << "Bin " << k;
  }
}

TEST(FftTest, RejectsNonPowerOfTwo)
{
  EXPECT_THROW(Fft(12), std::invalid_argument);
  EXPECT_THROW(Fft(1), std::invalid_argument);
}
```

In `tests/cpp/CMakeLists.txt` ist `fft_test.cpp` bereits eingetragen.

- [ ] **Step 3: Run test to verify it fails**

Run: `chmod +x scripts/test-cpp-in-container.sh && scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/Fft.h: No such file or directory`)

- [ ] **Step 4: Implementierung**

`src/core/Fft.h`:

```cpp
#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace spektra
{

// Radix-2-FFT fester Größe für reelle Eingaben. Nicht thread-sicher (interner Puffer).
class Fft
{
public:
  explicit Fft(size_t size);

  size_t Size() const { return m_n; }

  // input: Size() Werte; magnitudes: Size()/2 Werte, |X[k]| ohne Normierung.
  void Magnitudes(const float* input, float* magnitudes);

private:
  size_t m_n;
  std::vector<std::complex<float>> m_buffer;
  std::vector<std::complex<float>> m_twiddle;
  std::vector<size_t> m_reverse;
};

} // namespace spektra
```

`src/core/Fft.cpp`:

```cpp
#include "Fft.h"

#include <cmath>
#include <stdexcept>

namespace spektra
{

Fft::Fft(size_t size) : m_n(size)
{
  if (size < 2 || (size & (size - 1)) != 0)
    throw std::invalid_argument("FFT-Größe muss eine Zweierpotenz >= 2 sein");
  m_buffer.resize(m_n);
  m_twiddle.resize(m_n / 2);
  for (size_t k = 0; k < m_n / 2; ++k)
  {
    const double angle = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(m_n);
    m_twiddle[k] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
  }
  m_reverse.resize(m_n);
  size_t bits = 0;
  while ((size_t{1} << bits) < m_n)
    ++bits;
  for (size_t i = 0; i < m_n; ++i)
  {
    size_t r = 0;
    for (size_t b = 0; b < bits; ++b)
      if (i & (size_t{1} << b))
        r |= size_t{1} << (bits - 1 - b);
    m_reverse[i] = r;
  }
}

void Fft::Magnitudes(const float* input, float* magnitudes)
{
  for (size_t i = 0; i < m_n; ++i)
    m_buffer[m_reverse[i]] = {input[i], 0.0f};
  for (size_t len = 2; len <= m_n; len <<= 1)
  {
    const size_t half = len / 2, step = m_n / len;
    for (size_t start = 0; start < m_n; start += len)
      for (size_t j = 0; j < half; ++j)
      {
        const auto t = m_twiddle[j * step] * m_buffer[start + j + half];
        const auto u = m_buffer[start + j];
        m_buffer[start + j] = u + t;
        m_buffer[start + j + half] = u - t;
      }
  }
  for (size_t k = 0; k < m_n / 2; ++k)
    magnitudes[k] = std::abs(m_buffer[k]);
}

} // namespace spektra
```

- [ ] **Step 5: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: `100% tests passed, 0 tests failed out of 4`

- [ ] **Step 6: Stand prüfen (nicht committen)**

Run: `git status --short`

---

### Task 3: Spektrum wie der WebAudio-Analyser

**Files:**
- Create: `src/core/Spectrum.h`, `src/core/Spectrum.cpp`, `tests/cpp/spectrum_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt` (Quelle `Spectrum.cpp`, Test `spectrum_test.cpp`)

**Interfaces:**
- Consumes: `spektra::Fft` (Task 2)
- Produces:

```cpp
namespace spektra {
struct SpectrumConfig { size_t fftSize = 4096; float smoothing = 0.5f; float minDb = -100.0f; float maxDb = -30.0f; };
class Spectrum {
public:
  explicit Spectrum(SpectrumConfig config = {});
  size_t FftSize() const;
  size_t Bins() const;                         // FftSize()/2
  void Process(const float* samples);          // FftSize() Mono-Samples
  const std::vector<float>& Linear() const;    // lineare Amplitude je Bin, 0 bei Byte 0
  const std::vector<float>& Normalized() const;// Byte/255 je Bin
  void Reset();                                // Glättung vergessen
};
}
```

Algorithmus (WebAudio `AnalyserNode.getByteFrequencyData`): Blackman-Fenster `w[i] = 0.42 − 0.5·cos(2πi/N) + 0.08·cos(4πi/N)`; `|X[k]|/N`; Glättung `S = τ·S_alt + (1−τ)·|X|/N`; `dB = 20·log10(S)` (S = 0 → −∞); `byte = floor(255/(max−min)·(dB − min))`, begrenzt auf 0…255; `linear = byte == 0 ? 0 : 10^((min + (max−min)·byte/255 − max)/20)`.

- [ ] **Step 1: Failing test**

`tests/cpp/spectrum_test.cpp`:

```cpp
#include "core/Spectrum.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using spektra::Spectrum;

namespace
{
std::vector<float> Sine(double hz, float amplitude, size_t n = 4096, double rate = 44100.0)
{
  std::vector<float> s(n);
  for (size_t i = 0; i < n; ++i)
    s[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * hz * i / rate));
  return s;
}
} // namespace

TEST(SpectrumTest, SilenceIsZero)
{
  Spectrum spectrum;
  std::vector<float> silence(4096, 0.0f);
  spectrum.Process(silence.data());
  for (float v : spectrum.Linear())
    EXPECT_EQ(v, 0.0f);
}

TEST(SpectrumTest, LoudSineSaturatesItsBinOnly)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 1.0f);
  spectrum.Process(s.data());
  const size_t bin = static_cast<size_t>(std::round(1000.0 / (44100.0 / 4096.0)));
  EXPECT_FLOAT_EQ(spectrum.Linear()[bin], 1.0f);
  EXPECT_FLOAT_EQ(spectrum.Normalized()[bin], 1.0f);
  EXPECT_EQ(spectrum.Linear()[500], 0.0f);
}

TEST(SpectrumTest, QuietSineMapsThroughByteScale)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 0.001f);
  spectrum.Process(s.data());
  const size_t bin = 93;
  const float lin = spectrum.Linear()[bin];
  EXPECT_GT(lin, 0.002f);
  EXPECT_LT(lin, 0.02f);
  // Rückrechnung: Byte b -> 10^((-100 + 70 b/255 + 30)/20)
  const float b = spectrum.Normalized()[bin] * 255.0f;
  EXPECT_NEAR(lin, std::pow(10.0f, (-100.0f + 70.0f * b / 255.0f + 30.0f) / 20.0f), 1e-6f);
}

TEST(SpectrumTest, SmoothingDecaysAfterSilence)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 0.001f);
  std::vector<float> silence(4096, 0.0f);
  spectrum.Process(s.data());
  const float first = spectrum.Linear()[93];
  spectrum.Process(silence.data());
  const float second = spectrum.Linear()[93];
  EXPECT_LT(second, first);
  EXPECT_GT(second, 0.0f);
  spectrum.Reset();
  spectrum.Process(silence.data());
  EXPECT_EQ(spectrum.Linear()[93], 0.0f);
}
```

CMake: `${REPO_ROOT}/src/core/Spectrum.cpp` zu `CORE_SOURCES`, `spectrum_test.cpp` zu `TEST_SOURCES`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/Spectrum.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/core/Spectrum.h`:

```cpp
#pragma once

#include "Fft.h"

#include <cstddef>
#include <vector>

namespace spektra
{

struct SpectrumConfig
{
  size_t fftSize = 4096;
  float smoothing = 0.5f;
  float minDb = -100.0f;
  float maxDb = -30.0f;
};

// Nachbau von WebAudio AnalyserNode.getByteFrequencyData plus lineare Rückrechnung,
// damit die Analyse dieselben Zahlen sieht wie der Browser-Prototyp.
class Spectrum
{
public:
  explicit Spectrum(SpectrumConfig config = {});

  size_t FftSize() const { return m_config.fftSize; }
  size_t Bins() const { return m_config.fftSize / 2; }

  void Process(const float* samples);
  void Reset();

  const std::vector<float>& Linear() const { return m_linear; }
  const std::vector<float>& Normalized() const { return m_normalized; }

private:
  SpectrumConfig m_config;
  Fft m_fft;
  std::vector<float> m_window, m_windowed, m_magnitudes, m_smoothed, m_linear, m_normalized;
  float m_byteToLinear[256];
};

} // namespace spektra
```

`src/core/Spectrum.cpp`:

```cpp
#include "Spectrum.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

Spectrum::Spectrum(SpectrumConfig config) : m_config(config), m_fft(config.fftSize)
{
  const size_t n = m_config.fftSize;
  m_window.resize(n);
  for (size_t i = 0; i < n; ++i)
  {
    const double x = static_cast<double>(i) / static_cast<double>(n);
    m_window[i] = static_cast<float>(0.42 - 0.5 * std::cos(2.0 * M_PI * x) + 0.08 * std::cos(4.0 * M_PI * x));
  }
  m_windowed.resize(n);
  m_magnitudes.resize(n / 2);
  m_smoothed.assign(n / 2, 0.0f);
  m_linear.assign(n / 2, 0.0f);
  m_normalized.assign(n / 2, 0.0f);
  const float range = m_config.maxDb - m_config.minDb;
  for (int b = 0; b < 256; ++b)
    m_byteToLinear[b] = b == 0 ? 0.0f
                               : std::pow(10.0f, (m_config.minDb + range * b / 255.0f - m_config.maxDb) / 20.0f);
}

void Spectrum::Reset()
{
  std::fill(m_smoothed.begin(), m_smoothed.end(), 0.0f);
}

void Spectrum::Process(const float* samples)
{
  const size_t n = m_config.fftSize;
  for (size_t i = 0; i < n; ++i)
    m_windowed[i] = samples[i] * m_window[i];
  m_fft.Magnitudes(m_windowed.data(), m_magnitudes.data());
  const float tau = m_config.smoothing, range = m_config.maxDb - m_config.minDb;
  for (size_t k = 0; k < n / 2; ++k)
  {
    m_smoothed[k] = tau * m_smoothed[k] + (1.0f - tau) * m_magnitudes[k] / static_cast<float>(n);
    int byte = 0;
    if (m_smoothed[k] > 0.0f)
    {
      const float db = 20.0f * std::log10(m_smoothed[k]);
      byte = static_cast<int>(std::floor(255.0f / range * (db - m_config.minDb)));
      byte = std::clamp(byte, 0, 255);
    }
    m_normalized[k] = byte / 255.0f;
    m_linear[k] = m_byteToLinear[byte];
  }
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (8)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 4: AudioAnalyzer

**Files:**
- Create: `src/core/AudioAnalyzer.h`, `src/core/AudioAnalyzer.cpp`, `tests/cpp/audio_analyzer_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: Ausgaben von `Spectrum::Linear()` / `Normalized()` (Task 3)
- Produces:

```cpp
namespace spektra {
enum Band : int { kBass = 0, kLow = 1, kMid = 2, kHigh = 3 };
constexpr int kBandCount = 4;
constexpr int kFineBands = 32;
struct Features {
  std::array<float, kBandCount> level{};   // 0..1 (bereits mit M^0.6 skaliert)
  std::array<float, kBandCount> onset{};   // Stärke 0..1 in diesem Bild, sonst 0 (bereits · M)
  float centroid = 0.5f;                   // Tonhöhe der Mitten 0 (tief) .. 1 (hoch)
  float loudness = 0.0f;                   // Intensität 0..M
  bool drop = false;
  std::array<float, kFineBands> fine{};    // Feinspektrum 0..1
};
class AudioAnalyzer {
public:
  void SetIntensity(float m);              // M, auf 0.01..1 begrenzt; Standard 0.7
  // linear/normalized: je Bin; binHz = Abtastrate / FFT-Größe; t: Sekunden; dt: echte Zeit seit letztem Aufruf (<= 1)
  const Features& Update(const std::vector<float>& linear, const std::vector<float>& normalized,
                         float binHz, double t, float dt);
  void Rebase();                           // Langzeitmittel neu einpegeln
  const Features& Current() const;
};
}
```

Konstanten je Band (Spec §5.1): Bereich, Abklingzeit `rel`, Anschlag-Verhältnis `ratio`, Sperrzeit `cool`, Mittelungszeit `avgTau` (0,3 s; Mitten 0,6 s):

| Band | lo | hi | rel | ratio | cool | avgTau |
|---|---|---|---|---|---|---|
| Bass | 25 | 120 | 0.12 | 1.45 | 0.22 | 0.3 |
| Tiefen | 120 | 500 | 0.25 | 1.35 | 0.15 | 0.3 |
| Mitten | 500 | 3000 | 0.70 | 1.30 | 0.30 | 0.6 |
| Höhen | 3000 | 16000 | 0.08 | 1.50 | 0.07 | 0.3 |

- [ ] **Step 1: Failing tests**

`tests/cpp/audio_analyzer_test.cpp`:

```cpp
#include "core/AudioAnalyzer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace spektra;

namespace
{
constexpr float kBinHz = 44100.0f / 4096.0f;
constexpr float kDt = 1.0f / 60.0f;

// Spektrum mit konstantem Wert in [lo, hi) Hz, sonst 0.
std::vector<float> Bins(std::initializer_list<std::tuple<float, float, float>> ranges)
{
  std::vector<float> v(2048, 0.0f);
  for (const auto& [lo, hi, value] : ranges)
    for (size_t k = static_cast<size_t>(lo / kBinHz); k < v.size() && k * kBinHz < hi; ++k)
      v[k] = value;
  return v;
}

struct Runner
{
  AudioAnalyzer analyzer;
  double t = 0.0;
  const Features& Step(const std::vector<float>& linear)
  {
    t += kDt;
    std::vector<float> norm(linear.size());
    for (size_t i = 0; i < linear.size(); ++i)
      norm[i] = linear[i] > 0.0f ? 0.5f : 0.0f;
    return analyzer.Update(linear, norm, kBinHz, t, kDt);
  }
  void Run(const std::vector<float>& linear, double seconds)
  {
    for (int i = 0; i < static_cast<int>(seconds / kDt); ++i)
      Step(linear);
  }
};
} // namespace

TEST(AudioAnalyzerTest, SilenceGivesZeroLevels)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({}), 2.0);
  for (float l : r.analyzer.Current().level)
    EXPECT_EQ(l, 0.0f);
  EXPECT_FALSE(r.analyzer.Current().drop);
}

TEST(AudioAnalyzerTest, SteadyMidSettlesAtRelativeLevel)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{500.0f, 3000.0f, 0.2f}}), 10.0);
  const auto& f = r.analyzer.Current();
  EXPECT_NEAR(f.level[kMid], std::pow(1.0f / 1.7f, 1.3f), 0.02f);
  EXPECT_EQ(f.level[kBass], 0.0f);
  EXPECT_EQ(f.level[kHigh], 0.0f);
}

TEST(AudioAnalyzerTest, IntensityScalesLevels)
{
  Runner r;
  r.analyzer.SetIntensity(0.25f);
  r.Run(Bins({{500.0f, 3000.0f, 0.2f}}), 10.0);
  EXPECT_NEAR(r.analyzer.Current().level[kMid], std::pow(1.0f / 1.7f, 1.3f) * std::pow(0.25f, 0.6f), 0.02f);
}

TEST(AudioAnalyzerTest, KicksBecomeBassOnsets)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  // Kick nur bis 100 Hz, damit kein Bin in den Bereich der Tiefen (ab Bin 11) fällt.
  const auto quiet = Bins({{25.0f, 100.0f, 0.02f}});
  const auto kick = Bins({{25.0f, 100.0f, 0.5f}});
  r.Run(quiet, 2.0);
  int bassOnsets = 0, otherOnsets = 0;
  for (int beat = 0; beat < 8; ++beat)
  {
    for (int i = 0; i < 30; ++i) // 0,5 s pro Schlag, Kick die ersten 3 Bilder
    {
      const auto& f = r.Step(i < 3 ? kick : quiet);
      bassOnsets += f.onset[kBass] > 0.0f;
      otherOnsets += f.onset[kLow] > 0.0f || f.onset[kMid] > 0.0f || f.onset[kHigh] > 0.0f;
    }
  }
  EXPECT_EQ(bassOnsets, 8);
  EXPECT_EQ(otherOnsets, 0);
}

TEST(AudioAnalyzerTest, CentroidFollowsPitch)
{
  Runner low, high;
  low.analyzer.SetIntensity(1.0f);
  high.analyzer.SetIntensity(1.0f);
  std::vector<float> lowNorm(2048, 0.0f), highNorm(2048, 0.0f);
  for (int k = 46; k <= 50; ++k) lowNorm[k] = 0.8f;    // ~500 Hz
  for (int k = 270; k <= 278; ++k) highNorm[k] = 0.8f; // ~3 kHz
  std::vector<float> lin(2048, 0.0f);
  for (int i = 0; i < 120; ++i)
  {
    low.t += kDt; high.t += kDt;
    low.analyzer.Update(lin, lowNorm, kBinHz, low.t, kDt);
    high.analyzer.Update(lin, highNorm, kBinHz, high.t, kDt);
  }
  EXPECT_LT(low.analyzer.Current().centroid, 0.1f);
  EXPECT_GT(high.analyzer.Current().centroid, 0.9f);
}

TEST(AudioAnalyzerTest, KickAfterBasslessPassageIsDrop)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{500.0f, 3000.0f, 0.3f}}), 6.0); // Breakdown: nur Mitten
  const auto& f = r.Step(Bins({{25.0f, 120.0f, 0.6f}, {500.0f, 3000.0f, 0.3f}}));
  EXPECT_GT(f.onset[kBass], 0.35f);
  EXPECT_TRUE(f.drop);
}

TEST(AudioAnalyzerTest, NoDropWhileBassIsRunning)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  const auto quiet = Bins({{25.0f, 120.0f, 0.1f}, {500.0f, 3000.0f, 0.3f}});
  const auto kick = Bins({{25.0f, 120.0f, 0.6f}, {500.0f, 3000.0f, 0.3f}});
  bool anyDrop = false;
  for (int beat = 0; beat < 20; ++beat)
    for (int i = 0; i < 30; ++i)
      anyDrop |= r.Step(i < 3 ? kick : quiet).drop && beat > 4;
  EXPECT_FALSE(anyDrop);
}

TEST(AudioAnalyzerTest, RebaseResetsLongTermAverage)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{500.0f, 3000.0f, 0.8f}}), 20.0);
  r.analyzer.Rebase();
  r.Run(Bins({{500.0f, 3000.0f, 0.1f}}), 1.0);
  EXPECT_NEAR(r.analyzer.Current().level[kMid], std::pow(1.0f / 1.7f, 1.3f), 0.05f);
}

TEST(AudioAnalyzerTest, FineSpectrumFollowsBands)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{25.0f, 120.0f, 0.3f}}), 3.0);
  const auto& fine = r.analyzer.Current().fine;
  EXPECT_GT(fine[2], 0.2f);
  EXPECT_EQ(fine[30], 0.0f);
}
```

CMake: `AudioAnalyzer.cpp` und `audio_analyzer_test.cpp` eintragen.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/AudioAnalyzer.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/core/AudioAnalyzer.h`:

```cpp
#pragma once

#include <array>
#include <vector>

namespace spektra
{

enum Band : int
{
  kBass = 0,
  kLow = 1,
  kMid = 2,
  kHigh = 3
};
constexpr int kBandCount = 4;
constexpr int kFineBands = 32;

struct Features
{
  std::array<float, kBandCount> level{};
  std::array<float, kBandCount> onset{};
  float centroid = 0.5f;
  float loudness = 0.0f;
  bool drop = false;
  std::array<float, kFineBands> fine{};
};

// Pegel, Anschläge, Tonhöhe, Intensität und Drop wie im Prototyp (Spec §5).
class AudioAnalyzer
{
public:
  void SetIntensity(float m);
  float Intensity() const { return m_intensity; }

  const Features& Update(const std::vector<float>& linear, const std::vector<float>& normalized, float binHz,
                         double t, float dt);
  void Rebase();
  const Features& Current() const { return m_features; }

private:
  struct BandState
  {
    float longAvg = 0.0f;
    float avg = 0.0f;
    float prev = 0.0f;
    double last = -9.0;
  };

  float m_intensity = 0.7f;
  Features m_features;
  std::array<BandState, kBandCount> m_bands{};
  std::array<float, kFineBands> m_finePeak{};
  float m_loudAvg = 0.0f;
  float m_loudMax = 1e-3f;
  float m_bassHistory = 0.0f;
};

} // namespace spektra
```

`src/core/AudioAnalyzer.cpp`:

```cpp
#include "AudioAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace spektra
{
namespace
{

struct BandSpec
{
  float lo, hi, rel, ratio, cool, avgTau;
};

constexpr BandSpec kSpecs[kBandCount] = {
    {25.0f, 120.0f, 0.12f, 1.45f, 0.22f, 0.3f},
    {120.0f, 500.0f, 0.25f, 1.35f, 0.15f, 0.3f},
    {500.0f, 3000.0f, 0.70f, 1.30f, 0.30f, 0.6f},
    {3000.0f, 16000.0f, 0.08f, 1.50f, 0.07f, 0.3f},
};

constexpr float kFineSentinel = 0.05f; // „noch nicht eingepegelt“

float Clamp01(float v)
{
  return std::min(1.0f, std::max(0.0f, v));
}

float Approach(float dt, float tau)
{
  return 1.0f - std::exp(-dt / tau);
}

// RMS der linearen Amplituden zwischen lo und hi Hz.
float BandEnergy(const std::vector<float>& linear, float lo, float hi, float binHz)
{
  const size_t first = std::max<size_t>(1, static_cast<size_t>(std::floor(lo / binHz)));
  const size_t last = static_cast<size_t>(std::ceil(hi / binHz));
  double sum = 0.0;
  size_t n = 0;
  for (size_t b = first; b <= last && b < linear.size(); ++b, ++n)
    sum += static_cast<double>(linear[b]) * linear[b];
  return n ? static_cast<float>(std::sqrt(sum / n)) : 0.0f;
}

} // namespace

void AudioAnalyzer::SetIntensity(float m)
{
  m_intensity = std::min(1.0f, std::max(0.01f, m));
}

void AudioAnalyzer::Rebase()
{
  for (auto& b : m_bands)
    b.longAvg = 0.0f;
  m_finePeak.fill(kFineSentinel);
}

const Features& AudioAnalyzer::Update(const std::vector<float>& linear, const std::vector<float>& normalized,
                                      float binHz, double t, float dt)
{
  const float m = m_intensity, gain = std::pow(m, 0.6f);
  if (m_finePeak[0] == 0.0f)
    m_finePeak.fill(kFineSentinel);

  float loud = 0.0f;
  for (int i = 0; i < kBandCount; ++i)
  {
    const auto& spec = kSpecs[i];
    auto& s = m_bands[i];
    const float e = BandEnergy(linear, spec.lo, spec.hi, binHz);
    loud += e;
    // Pegel relativ zum eigenen Langzeitmittel; weit darunter: schnell aufholen.
    if (s.longAvg == 0.0f)
      s.longAvg = e;
    s.longAvg += (e - s.longAvg) * Approach(dt, e > s.longAvg * 2.5f ? 0.8f : 8.0f);
    const float norm = e < 1e-4f ? 0.0f : Clamp01(e / (s.longAvg * 1.7f + 1e-4f));
    const float target = std::pow(norm, 1.3f) * gain;
    float& level = m_features.level[i];
    level += (target - level) * (target > level ? 0.6f : Approach(dt, spec.rel));

    m_features.onset[i] = 0.0f;
    if (e > s.avg * spec.ratio && e > s.longAvg * 0.5f && e > s.prev && t - s.last > spec.cool)
    {
      s.last = t;
      m_features.onset[i] = Clamp01((e / std::max(s.avg, 1e-4f) - 1.0f) * 0.9f) * m;
    }
    s.avg += (e - s.avg) * Approach(dt, spec.avgTau);
    s.prev = e;
  }

  // Tonhöhe: Schwerpunkt der Mitten auf der Byte-Skala, logarithmisch 500 Hz -> 0, 3 kHz -> 1.
  double weighted = 0.0, weights = 0.0;
  const size_t first = static_cast<size_t>(std::floor(500.0f / binHz));
  const size_t last = static_cast<size_t>(std::ceil(3000.0f / binHz));
  for (size_t b = std::max<size_t>(first, 1); b <= last && b < normalized.size(); ++b)
  {
    const double v = normalized[b], w = v * v;
    weighted += w * std::log(b * binHz / 500.0) / std::log(6.0);
    weights += w;
  }
  if (weights > 0.01)
    m_features.centroid += (Clamp01(static_cast<float>(weighted / weights)) - m_features.centroid) * 0.15f;

  // Feinspektrum: 32 logarithmische Bänder 30 Hz .. 14 kHz.
  for (int i = 0; i < kFineBands; ++i)
  {
    const float lo = 30.0f * std::pow(14000.0f / 30.0f, static_cast<float>(i) / kFineBands);
    const float hi = 30.0f * std::pow(14000.0f / 30.0f, static_cast<float>(i + 1) / kFineBands);
    const float e = BandEnergy(linear, lo, hi, binHz);
    float& peak = m_finePeak[i];
    if (peak == kFineSentinel)
      peak = e + 1e-4f;
    peak += (e - peak) * Approach(dt, 8.0f);
    const float target = (e < 1e-4f ? 0.0f : std::pow(Clamp01(e / (peak * 1.7f + 1e-4f)), 1.4f)) * gain;
    float& fine = m_features.fine[i];
    fine += (target - fine) * (target > fine ? 0.6f : 0.12f);
  }

  // Intensität relativ zum lautesten Moment der letzten ~40 s.
  loud /= kBandCount;
  m_loudAvg += (loud - m_loudAvg) * Approach(dt, 1.5f);
  m_loudMax = std::max({m_loudAvg, m_loudMax * std::exp(-dt / 40.0f), 1e-3f});
  const float loudTarget = m * std::pow(Clamp01(m_loudAvg / m_loudMax), 2.0f);
  m_features.loudness += (loudTarget - m_features.loudness) * Approach(dt, 0.8f);

  // Drop: kräftiger Kick nach einer Phase ohne Bass, während die Musik laut ist.
  m_features.drop = m_features.onset[kBass] > 0.35f * m && m_bassHistory < 0.15f && m_loudAvg > 0.5f * m_loudMax;
  m_bassHistory += (m_features.level[kBass] - m_bassHistory) * Approach(dt, 3.0f);

  return m_features;
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (17). Scheitert `KicksBecomeBassOnsets` mit 7 statt 8, liegt das am ersten Kick direkt nach der Einpegelphase: dann prüfen, ob `longAvg` nach 2 s Ruhe-Bass gleich 0,02 ist und `e > 0,5·longAvg` erfüllt – die Formel ist identisch zum Prototyp und darf nicht geändert werden; stattdessen die Einpegelphase im Test auf 3 s verlängern.

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 5: Einstellungen, Zuordnung, `live.json`, `renderer.json`, `settings.xml`

**Files:**
- Create: `src/core/Settings.h`, `src/core/Settings.cpp`, `src/core/Routing.h`, `src/core/Routing.cpp`, `src/core/StateFiles.h`, `src/core/StateFiles.cpp`, `tests/cpp/settings_test.cpp`, `tests/cpp/routing_test.cpp`, `tests/cpp/state_files_test.cpp`, `visualization.spektra/resources/settings.xml`, `visualization.spektra/resources/language/resource.language.de_de/strings.po`, `visualization.spektra/resources/language/resource.language.en_gb/strings.po`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `Features`, `kBandCount` (Task 4)
- Produces:

```cpp
namespace spektra {
enum Effect : int { kBreath, kFlame, kCorona, kFree, kFlow, kWave, kBurst, kJet, kStreamer, kDust };
constexpr int kEffectCount = 10;
const char* EffectKey(int effect);   // "breath", "flame", "corona", "free", "flow", "wave", "burst", "jet", "streamer", "dust"
const char* BandKey(int band);       // "bass", "low", "mid", "high"
constexpr float kPixelSteps[9] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};

struct Settings {
  bool enabled = true; bool autoopen = true; std::string scene = "eclipse"; int intensity = 70;
  int flame = 100, waves = 100, particles = 100, corona = 100;
  int pixel = 2; bool drop = true;
  std::array<std::array<bool, kBandCount>, kEffectCount> route{};   // [effect][band]
  int maxParticles = 42000;
  static Settings Defaults();
};
enum class SettingType { Bool, Int, String };
enum class ApplyResult { Ok, UnknownKey, InvalidValue };
const std::vector<std::string>& SettingKeys();           // alle Schlüssel in fester Reihenfolge
std::optional<SettingType> TypeOf(const std::string& key);
ApplyResult ApplySetting(Settings& settings, const std::string& key, const std::string& value);
std::string SettingValue(const Settings& settings, const std::string& key);   // Text, wie ApplySetting ihn annimmt

float RoutedLevel(const Settings&, const Features&, int effect);   // stärkstes zugeordnetes Band, sonst 0
float RoutedOnset(const Settings&, const Features&, int effect);

struct LiveSettings { int64_t revision = 0; Settings settings; };
std::optional<LiveSettings> ReadLiveSettings(const std::string& path, const Settings& base);
struct RendererStatus { std::string state = "running"; std::string scene = "eclipse"; float fps = 0.0f; std::string error; };
bool WriteRendererStatus(const std::string& path, const RendererStatus& status);
}
```

Schlüssel (Spec §7): `enabled`, `autoopen`, `scene`, `intensity`, `eclipse.flame`, `eclipse.waves`, `eclipse.particles`, `eclipse.corona`, `eclipse.pixel`, `eclipse.drop`, `eclipse.route.<effect>.<band>` (40), `maxparticles`. Bereiche: `intensity` 1–100; Elemente 0–100; `eclipse.pixel` 0–8; `maxparticles` 10000–60000 (Zahlen außerhalb werden begrenzt); `scene` nur `eclipse`; Bool nur `true`/`false`; Zahl muss vollständig eine ganze Zahl sein.

Standard-Zuordnung: breath→bass, flame→low, corona→mid, free→high, flow→high, wave→bass, burst→mid, jet→low, streamer→mid, dust→high.

`live.json`: `{"revision": <int>, "settings": {"<schlüssel>": <bool|int|string>, …}}`; Werte werden in Text umgewandelt (`true`/`false`, ganze Zahl, Text) und per `ApplySetting` angewendet; unbekannte Schlüssel und ungültige Werte werden übersprungen.

`renderer.json`: `{"state": "...", "scene": "...", "fps": <zahl>, "error": "...", "time": <unix-sekunden>}`, atomar über `path + ".tmp"` und `rename`.

- [ ] **Step 1: Failing tests**

`tests/cpp/settings_test.cpp`:

```cpp
#include "TempDir.h"
#include "core/Settings.h"
#include "core/StateFiles.h"

#include <gtest/gtest.h>

#include <fstream>

using namespace spektra;

namespace
{
void Write(const std::string& path, const std::string& text)
{
  std::ofstream(path) << text;
}
} // namespace

TEST(SettingsTest, DefaultsMatchSpec)
{
  const auto s = Settings::Defaults();
  EXPECT_TRUE(s.enabled);
  EXPECT_TRUE(s.autoopen);
  EXPECT_EQ(s.scene, "eclipse");
  EXPECT_EQ(s.intensity, 70);
  EXPECT_EQ(s.flame, 100);
  EXPECT_EQ(s.waves, 100);
  EXPECT_EQ(s.particles, 100);
  EXPECT_EQ(s.corona, 100);
  EXPECT_EQ(s.pixel, 2);
  EXPECT_FLOAT_EQ(kPixelSteps[s.pixel], 1.0f);
  EXPECT_TRUE(s.drop);
  EXPECT_EQ(s.maxParticles, 42000);
  const int expected[kEffectCount] = {kBass, kLow, kMid, kHigh, kHigh, kBass, kMid, kLow, kMid, kHigh};
  for (int e = 0; e < kEffectCount; ++e)
    for (int b = 0; b < kBandCount; ++b)
      EXPECT_EQ(s.route[e][b], b == expected[e]) << EffectKey(e) << "/" << BandKey(b);
}

TEST(SettingsTest, KeysAreCompleteAndTyped)
{
  EXPECT_EQ(SettingKeys().size(), 10u + 40u + 1u);
  EXPECT_EQ(TypeOf("intensity"), SettingType::Int);
  EXPECT_EQ(TypeOf("eclipse.route.wave.bass"), SettingType::Bool);
  EXPECT_EQ(TypeOf("scene"), SettingType::String);
  EXPECT_FALSE(TypeOf("eclipse.route.wave.treble").has_value());
}

TEST(SettingsTest, ApplyParsesAndClamps)
{
  auto s = Settings::Defaults();
  EXPECT_EQ(ApplySetting(s, "intensity", "40"), ApplyResult::Ok);
  EXPECT_EQ(s.intensity, 40);
  EXPECT_EQ(ApplySetting(s, "intensity", "0"), ApplyResult::Ok);
  EXPECT_EQ(s.intensity, 1);
  EXPECT_EQ(ApplySetting(s, "eclipse.flame", "250"), ApplyResult::Ok);
  EXPECT_EQ(s.flame, 100);
  EXPECT_EQ(ApplySetting(s, "eclipse.pixel", "8"), ApplyResult::Ok);
  EXPECT_EQ(s.pixel, 8);
  EXPECT_EQ(ApplySetting(s, "eclipse.route.wave.high", "true"), ApplyResult::Ok);
  EXPECT_TRUE(s.route[kWave][kHigh]);
  EXPECT_EQ(ApplySetting(s, "maxparticles", "5"), ApplyResult::Ok);
  EXPECT_EQ(s.maxParticles, 10000);
}

TEST(SettingsTest, ApplyRejectsGarbage)
{
  auto s = Settings::Defaults();
  EXPECT_EQ(ApplySetting(s, "intensity", "viel"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "intensity", "40x"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "eclipse.drop", "ja"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "scene", "matrix"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "farbe", "rot"), ApplyResult::UnknownKey);
  EXPECT_EQ(s.intensity, 70);
}

TEST(SettingsTest, ValueRoundTrips)
{
  auto s = Settings::Defaults();
  for (const auto& key : SettingKeys())
  {
    auto copy = Settings::Defaults();
    EXPECT_EQ(ApplySetting(copy, key, SettingValue(s, key)), ApplyResult::Ok) << key;
    EXPECT_EQ(SettingValue(copy, key), SettingValue(s, key)) << key;
  }
}

TEST(SettingsTest, LiveJsonIsApplied)
{
  TempDir dir;
  Write(dir.File("live.json"),
        R"({"revision": 7, "settings": {"intensity": 25, "eclipse.drop": false, "eclipse.route.burst.bass": true,
            "scene": "eclipse", "unbekannt": 3}})");
  const auto live = ReadLiveSettings(dir.File("live.json"), Settings::Defaults());
  ASSERT_TRUE(live.has_value());
  EXPECT_EQ(live->revision, 7);
  EXPECT_EQ(live->settings.intensity, 25);
  EXPECT_FALSE(live->settings.drop);
  EXPECT_TRUE(live->settings.route[kBurst][kBass]);
  EXPECT_TRUE(live->settings.route[kBurst][kMid]);
}

TEST(SettingsTest, LiveJsonGarbageIsIgnored)
{
  TempDir dir;
  auto base = Settings::Defaults();
  base.intensity = 33;
  EXPECT_FALSE(ReadLiveSettings(dir.File("fehlt.json"), base).has_value());
  Write(dir.File("a.json"), "kein json");
  EXPECT_FALSE(ReadLiveSettings(dir.File("a.json"), base).has_value());
  Write(dir.File("b.json"), R"({"settings": {"intensity": 10}})");
  EXPECT_FALSE(ReadLiveSettings(dir.File("b.json"), base).has_value());
  Write(dir.File("c.json"), R"({"revision": 2, "settings": {"intensity": "zehn", "eclipse.flame": [1]}})");
  const auto live = ReadLiveSettings(dir.File("c.json"), base);
  ASSERT_TRUE(live.has_value());
  EXPECT_EQ(live->settings.intensity, 33);
  EXPECT_EQ(live->settings.flame, 100);
}
```

`tests/cpp/routing_test.cpp`:

```cpp
#include "core/Routing.h"

#include <gtest/gtest.h>

using namespace spektra;

TEST(RoutingTest, DefaultRouteTakesAssignedBand)
{
  const auto s = Settings::Defaults();
  Features f;
  f.level = {0.1f, 0.2f, 0.3f, 0.4f};
  f.onset = {0.5f, 0.0f, 0.7f, 0.0f};
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kFlame), 0.2f);
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kCorona), 0.3f);
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kWave), 0.5f);
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kBurst), 0.7f);
}

TEST(RoutingTest, StrongestOfSeveralBandsWins)
{
  auto s = Settings::Defaults();
  s.route[kWave][kHigh] = true;
  Features f;
  f.onset = {0.2f, 0.0f, 0.0f, 0.9f};
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kWave), 0.9f);
}

TEST(RoutingTest, NoBandMeansOff)
{
  auto s = Settings::Defaults();
  s.route[kFlame] = {false, false, false, false};
  Features f;
  f.level = {1.0f, 1.0f, 1.0f, 1.0f};
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kFlame), 0.0f);
}
```

`tests/cpp/state_files_test.cpp`:

```cpp
#include "TempDir.h"
#include "core/StateFiles.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace spektra;

TEST(StateFilesTest, RendererStatusIsWrittenAtomically)
{
  TempDir dir;
  const auto path = dir.File("sub/renderer.json");
  RendererStatus status;
  status.fps = 58.5f;
  ASSERT_TRUE(WriteRendererStatus(path, status));
  EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
  std::ifstream in(path);
  const auto json = nlohmann::json::parse(in);
  EXPECT_EQ(json["state"], "running");
  EXPECT_EQ(json["scene"], "eclipse");
  EXPECT_NEAR(json["fps"].get<double>(), 58.5, 1e-3);
  EXPECT_EQ(json["error"], "");
  EXPECT_GT(json["time"].get<int64_t>(), 1700000000);
}
```

CMake: Quellen `Settings.cpp`, `Routing.cpp`, `StateFiles.cpp`; Tests `settings_test.cpp`, `routing_test.cpp`, `state_files_test.cpp`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/Settings.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/core/Settings.h`:

```cpp
#pragma once

#include "AudioAnalyzer.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace spektra
{

enum Effect : int
{
  kBreath,
  kFlame,
  kCorona,
  kFree,
  kFlow,
  kWave,
  kBurst,
  kJet,
  kStreamer,
  kDust
};
constexpr int kEffectCount = 10;
constexpr float kPixelSteps[9] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};

const char* EffectKey(int effect);
const char* BandKey(int band);

struct Settings
{
  bool enabled = true;
  bool autoopen = true;
  std::string scene = "eclipse";
  int intensity = 70;
  int flame = 100;
  int waves = 100;
  int particles = 100;
  int corona = 100;
  int pixel = 2;
  bool drop = true;
  std::array<std::array<bool, kBandCount>, kEffectCount> route{};
  int maxParticles = 42000;

  static Settings Defaults();
};

enum class SettingType
{
  Bool,
  Int,
  String
};

enum class ApplyResult
{
  Ok,
  UnknownKey,
  InvalidValue
};

const std::vector<std::string>& SettingKeys();
std::optional<SettingType> TypeOf(const std::string& key);
ApplyResult ApplySetting(Settings& settings, const std::string& key, const std::string& value);
std::string SettingValue(const Settings& settings, const std::string& key);

} // namespace spektra
```

`src/core/Settings.cpp`:

```cpp
#include "Settings.h"

#include <algorithm>
#include <cstdlib>
#include <cerrno>

namespace spektra
{
namespace
{

constexpr const char* kEffectKeys[kEffectCount] = {"breath", "flame", "corona", "free",     "flow",
                                                  "wave",   "burst", "jet",    "streamer", "dust"};
constexpr const char* kBandKeys[kBandCount] = {"bass", "low", "mid", "high"};
constexpr const char* kRoutePrefix = "eclipse.route.";

struct IntSpec
{
  const char* key;
  int Settings::*field;
  int min, max;
};

constexpr IntSpec kInts[] = {
    {"intensity", &Settings::intensity, 1, 100},
    {"eclipse.flame", &Settings::flame, 0, 100},
    {"eclipse.waves", &Settings::waves, 0, 100},
    {"eclipse.particles", &Settings::particles, 0, 100},
    {"eclipse.corona", &Settings::corona, 0, 100},
    {"eclipse.pixel", &Settings::pixel, 0, 8},
    {"maxparticles", &Settings::maxParticles, 10000, 60000},
};

struct BoolSpec
{
  const char* key;
  bool Settings::*field;
};

constexpr BoolSpec kBools[] = {
    {"enabled", &Settings::enabled},
    {"autoopen", &Settings::autoopen},
    {"eclipse.drop", &Settings::drop},
};

// "eclipse.route.<effekt>.<band>" -> {effekt, band}
std::optional<std::pair<int, int>> ParseRoute(const std::string& key)
{
  if (key.rfind(kRoutePrefix, 0) != 0)
    return std::nullopt;
  const auto rest = key.substr(std::char_traits<char>::length(kRoutePrefix));
  const auto dot = rest.find('.');
  if (dot == std::string::npos)
    return std::nullopt;
  const auto effect = rest.substr(0, dot), band = rest.substr(dot + 1);
  for (int e = 0; e < kEffectCount; ++e)
    for (int b = 0; b < kBandCount; ++b)
      if (effect == kEffectKeys[e] && band == kBandKeys[b])
        return std::make_pair(e, b);
  return std::nullopt;
}

std::optional<bool> ParseBool(const std::string& value)
{
  if (value == "true")
    return true;
  if (value == "false")
    return false;
  return std::nullopt;
}

std::optional<long> ParseInt(const std::string& value)
{
  if (value.empty())
    return std::nullopt;
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (errno != 0 || end == nullptr || *end != '\0')
    return std::nullopt;
  return parsed;
}

} // namespace

const char* EffectKey(int effect)
{
  return kEffectKeys[effect];
}

const char* BandKey(int band)
{
  return kBandKeys[band];
}

Settings Settings::Defaults()
{
  Settings s;
  const int bands[kEffectCount] = {kBass, kLow, kMid, kHigh, kHigh, kBass, kMid, kLow, kMid, kHigh};
  for (int e = 0; e < kEffectCount; ++e)
    s.route[e][bands[e]] = true;
  return s;
}

const std::vector<std::string>& SettingKeys()
{
  static const std::vector<std::string> keys = [] {
    std::vector<std::string> k = {"enabled",           "autoopen",       "scene",         "intensity",
                                  "eclipse.flame",     "eclipse.waves",  "eclipse.particles",
                                  "eclipse.corona",    "eclipse.pixel",  "eclipse.drop"};
    for (int e = 0; e < kEffectCount; ++e)
      for (int b = 0; b < kBandCount; ++b)
        k.push_back(std::string(kRoutePrefix) + kEffectKeys[e] + "." + kBandKeys[b]);
    k.push_back("maxparticles");
    return k;
  }();
  return keys;
}

std::optional<SettingType> TypeOf(const std::string& key)
{
  if (key == "scene")
    return SettingType::String;
  for (const auto& spec : kInts)
    if (key == spec.key)
      return SettingType::Int;
  for (const auto& spec : kBools)
    if (key == spec.key)
      return SettingType::Bool;
  if (ParseRoute(key))
    return SettingType::Bool;
  return std::nullopt;
}

ApplyResult ApplySetting(Settings& settings, const std::string& key, const std::string& value)
{
  if (key == "scene")
  {
    if (value != "eclipse")
      return ApplyResult::InvalidValue;
    settings.scene = value;
    return ApplyResult::Ok;
  }
  for (const auto& spec : kInts)
    if (key == spec.key)
    {
      const auto parsed = ParseInt(value);
      if (!parsed)
        return ApplyResult::InvalidValue;
      settings.*spec.field = static_cast<int>(std::clamp<long>(*parsed, spec.min, spec.max));
      return ApplyResult::Ok;
    }
  for (const auto& spec : kBools)
    if (key == spec.key)
    {
      const auto parsed = ParseBool(value);
      if (!parsed)
        return ApplyResult::InvalidValue;
      settings.*spec.field = *parsed;
      return ApplyResult::Ok;
    }
  if (const auto route = ParseRoute(key))
  {
    const auto parsed = ParseBool(value);
    if (!parsed)
      return ApplyResult::InvalidValue;
    settings.route[route->first][route->second] = *parsed;
    return ApplyResult::Ok;
  }
  return ApplyResult::UnknownKey;
}

std::string SettingValue(const Settings& settings, const std::string& key)
{
  if (key == "scene")
    return settings.scene;
  for (const auto& spec : kInts)
    if (key == spec.key)
      return std::to_string(settings.*spec.field);
  for (const auto& spec : kBools)
    if (key == spec.key)
      return settings.*spec.field ? "true" : "false";
  if (const auto route = ParseRoute(key))
    return settings.route[route->first][route->second] ? "true" : "false";
  return "";
}

} // namespace spektra
```

`src/core/Routing.h`:

```cpp
#pragma once

#include "AudioAnalyzer.h"
#include "Settings.h"

namespace spektra
{

// Pegel bzw. Anschlag eines Effekts: das stärkste zugeordnete Band, ohne Zuordnung 0.
float RoutedLevel(const Settings& settings, const Features& features, int effect);
float RoutedOnset(const Settings& settings, const Features& features, int effect);

} // namespace spektra
```

`src/core/Routing.cpp`:

```cpp
#include "Routing.h"

#include <algorithm>

namespace spektra
{

float RoutedLevel(const Settings& settings, const Features& features, int effect)
{
  float v = 0.0f;
  for (int b = 0; b < kBandCount; ++b)
    if (settings.route[effect][b])
      v = std::max(v, features.level[b]);
  return v;
}

float RoutedOnset(const Settings& settings, const Features& features, int effect)
{
  float v = 0.0f;
  for (int b = 0; b < kBandCount; ++b)
    if (settings.route[effect][b])
      v = std::max(v, features.onset[b]);
  return v;
}

} // namespace spektra
```

`src/core/StateFiles.h`:

```cpp
#pragma once

#include "Settings.h"

#include <cstdint>
#include <optional>
#include <string>

namespace spektra
{

// live.json (vom Service geschrieben, Spec §7.1).
struct LiveSettings
{
  int64_t revision = 0;
  Settings settings;
};

// std::nullopt, wenn die Datei fehlt, kein Objekt ist oder keine ganzzahlige "revision" hat.
// Einträge in "settings" werden auf base angewendet; Unbekanntes und Ungültiges wird übersprungen.
std::optional<LiveSettings> ReadLiveSettings(const std::string& path, const Settings& base);

// renderer.json (Herzschlag für die Recovery im Service, Spec §8.1).
struct RendererStatus
{
  std::string state = "running"; // running | error
  std::string scene = "eclipse";
  float fps = 0.0f;
  std::string error;
};

// Schreibt atomar (path + ".tmp", dann rename) und legt das Verzeichnis bei Bedarf an.
bool WriteRendererStatus(const std::string& path, const RendererStatus& status);

} // namespace spektra
```

`src/core/StateFiles.cpp`:

```cpp
#include "StateFiles.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace spektra
{

std::optional<LiveSettings> ReadLiveSettings(const std::string& path, const Settings& base)
{
  std::ifstream in(path);
  if (!in)
    return std::nullopt;
  const auto json = nlohmann::json::parse(in, nullptr, false);
  if (json.is_discarded() || !json.is_object())
    return std::nullopt;
  const auto revision = json.find("revision");
  if (revision == json.end() || !revision->is_number_integer())
    return std::nullopt;
  LiveSettings live;
  live.revision = revision->get<int64_t>();
  live.settings = base;
  const auto settings = json.find("settings");
  if (settings != json.end() && settings->is_object())
    for (const auto& [key, value] : settings->items())
    {
      std::string text;
      if (value.is_boolean())
        text = value.get<bool>() ? "true" : "false";
      else if (value.is_number_integer())
        text = std::to_string(value.get<int64_t>());
      else if (value.is_string())
        text = value.get<std::string>();
      else
        continue;
      ApplySetting(live.settings, key, text);
    }
  return live;
}

bool WriteRendererStatus(const std::string& path, const RendererStatus& status)
{
  std::error_code ec;
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty())
    std::filesystem::create_directories(parent, ec);
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  const nlohmann::json json = {
      {"state", status.state}, {"scene", status.scene}, {"fps", status.fps}, {"error", status.error}, {"time", now}};
  const auto temporary = path + ".tmp";
  {
    std::ofstream out(temporary, std::ios::trunc);
    if (!out)
      return false;
    out << json.dump();
    if (!out.flush())
      return false;
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    std::remove(temporary.c_str());
    return false;
  }
  return true;
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (28)

- [ ] **Step 5: Einstellungsdialog `settings.xml` und Texte**

Kodi-Format wie Party Video (altes Format, unterstützt Punkt-IDs). Die Zuordnung steht pro Effekt unter einer Zwischenüberschrift (`lsep`).

`visualization.spektra/resources/settings.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<settings>
  <category label="32000">
    <setting id="enabled" type="bool" label="32001" default="true" />
    <setting id="autoopen" type="bool" label="32002" default="true" />
    <setting id="scene" type="select" label="32003" values="eclipse" default="eclipse" />
    <setting id="intensity" type="slider" label="32004" range="1,1,100" option="int" default="70" />
  </category>
  <category label="32010">
    <setting id="eclipse.flame" type="slider" label="32011" range="0,1,100" option="int" default="100" />
    <setting id="eclipse.waves" type="slider" label="32012" range="0,1,100" option="int" default="100" />
    <setting id="eclipse.particles" type="slider" label="32013" range="0,1,100" option="int" default="100" />
    <setting id="eclipse.corona" type="slider" label="32014" range="0,1,100" option="int" default="100" />
    <setting id="eclipse.pixel" type="enum" label="32015" lvalues="32020|32021|32022|32023|32024|32025|32026|32027|32028" default="2" />
    <setting id="eclipse.drop" type="bool" label="32016" default="true" />
  </category>
  <category label="32030">
    <setting type="lsep" label="32031" />
    <setting id="eclipse.route.breath.bass" type="bool" label="32050" default="true" />
    <setting id="eclipse.route.breath.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.breath.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.breath.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32032" />
    <setting id="eclipse.route.flame.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.flame.low" type="bool" label="32051" default="true" />
    <setting id="eclipse.route.flame.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.flame.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32033" />
    <setting id="eclipse.route.corona.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.corona.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.corona.mid" type="bool" label="32052" default="true" />
    <setting id="eclipse.route.corona.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32034" />
    <setting id="eclipse.route.free.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.free.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.free.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.free.high" type="bool" label="32053" default="true" />
    <setting type="lsep" label="32035" />
    <setting id="eclipse.route.flow.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.flow.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.flow.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.flow.high" type="bool" label="32053" default="true" />
    <setting type="lsep" label="32036" />
    <setting id="eclipse.route.wave.bass" type="bool" label="32050" default="true" />
    <setting id="eclipse.route.wave.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.wave.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.wave.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32037" />
    <setting id="eclipse.route.burst.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.burst.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.burst.mid" type="bool" label="32052" default="true" />
    <setting id="eclipse.route.burst.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32038" />
    <setting id="eclipse.route.jet.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.jet.low" type="bool" label="32051" default="true" />
    <setting id="eclipse.route.jet.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.jet.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32039" />
    <setting id="eclipse.route.streamer.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.streamer.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.streamer.mid" type="bool" label="32052" default="true" />
    <setting id="eclipse.route.streamer.high" type="bool" label="32053" default="false" />
    <setting type="lsep" label="32040" />
    <setting id="eclipse.route.dust.bass" type="bool" label="32050" default="false" />
    <setting id="eclipse.route.dust.low" type="bool" label="32051" default="false" />
    <setting id="eclipse.route.dust.mid" type="bool" label="32052" default="false" />
    <setting id="eclipse.route.dust.high" type="bool" label="32053" default="true" />
  </category>
  <category label="32060">
    <setting id="maxparticles" type="slider" label="32061" range="10000,1000,60000" option="int" default="42000" />
  </category>
</settings>
```

`visualization.spektra/resources/language/resource.language.de_de/strings.po`:

```
# Spektra – deutsche Texte
msgid ""
msgstr ""
"Language: de_DE\n"
"Content-Type: text/plain; charset=UTF-8\n"

msgctxt "#32000"
msgid "General"
msgstr "Allgemein"

msgctxt "#32001"
msgid "Spektra on"
msgstr "Spektra eingeschaltet"

msgctxt "#32002"
msgid "Open automatically when music starts"
msgstr "Bei Musikstart automatisch öffnen"

msgctxt "#32003"
msgid "Scene"
msgstr "Szene"

msgctxt "#32004"
msgid "Music intensity (%)"
msgstr "Musik-Intensität (%)"

msgctxt "#32010"
msgid "Eclipse"
msgstr "Eclipse"

msgctxt "#32011"
msgid "Flame (%)"
msgstr "Flamme (%)"

msgctxt "#32012"
msgid "Waves (%)"
msgstr "Wellen (%)"

msgctxt "#32013"
msgid "Free particles (%)"
msgstr "Freie Partikel (%)"

msgctxt "#32014"
msgid "Corona (%)"
msgstr "Corona (%)"

msgctxt "#32015"
msgid "Pixel size"
msgstr "Pixelgröße"

msgctxt "#32016"
msgid "Drop effect"
msgstr "Drop-Effekt"

msgctxt "#32020"
msgid "0.5×"
msgstr "0,5×"

msgctxt "#32021"
msgid "0.75×"
msgstr "0,75×"

msgctxt "#32022"
msgid "1× (default)"
msgstr "1× (Standard)"

msgctxt "#32023"
msgid "1.5×"
msgstr "1,5×"

msgctxt "#32024"
msgid "2×"
msgstr "2×"

msgctxt "#32025"
msgid "3×"
msgstr "3×"

msgctxt "#32026"
msgid "4×"
msgstr "4×"

msgctxt "#32027"
msgid "6×"
msgstr "6×"

msgctxt "#32028"
msgid "8×"
msgstr "8×"

msgctxt "#32030"
msgid "Eclipse: mapping"
msgstr "Eclipse: Zuordnung"

msgctxt "#32031"
msgid "Ring breathes (level)"
msgstr "Ring atmet (Pegel)"

msgctxt "#32032"
msgid "Flame height (level)"
msgstr "Flammenhöhe (Pegel)"

msgctxt "#32033"
msgid "Corona strength (level)"
msgstr "Corona-Stärke (Pegel)"

msgctxt "#32034"
msgid "Dust breaks free (level)"
msgstr "Staub löst sich (Pegel)"

msgctxt "#32035"
msgid "Flow & shimmer (level)"
msgstr "Strömung & Flimmern (Pegel)"

msgctxt "#32036"
msgid "Wave (hit)"
msgstr "Welle (Anschlag)"

msgctxt "#32037"
msgid "Flame burst all around (hit)"
msgstr "Flammenstoß rundum (Anschlag)"

msgctxt "#32038"
msgid "Flame jet (hit)"
msgstr "Stichflamme (Anschlag)"

msgctxt "#32039"
msgid "Streamer + arc (hit)"
msgstr "Streamer + Bogen (Anschlag)"

msgctxt "#32040"
msgid "Dust burst (hit)"
msgstr "Staubausbruch (Anschlag)"

msgctxt "#32050"
msgid "Bass (25–120 Hz)"
msgstr "Bass (25–120 Hz)"

msgctxt "#32051"
msgid "Lows (120–500 Hz)"
msgstr "Tiefen (120–500 Hz)"

msgctxt "#32052"
msgid "Mids (500 Hz–3 kHz)"
msgstr "Mitten (500 Hz–3 kHz)"

msgctxt "#32053"
msgid "Highs (3–16 kHz)"
msgstr "Höhen (3–16 kHz)"

msgctxt "#32060"
msgid "Performance"
msgstr "Leistung"

msgctxt "#32061"
msgid "Particle limit"
msgstr "Teilchenlimit"
```

`visualization.spektra/resources/language/resource.language.en_gb/strings.po`: gleiche `msgctxt`/`msgid`-Einträge, `msgstr ""` überall, Kopf `"Language: en_GB\n"`.

- [ ] **Step 6: Konsistenz Standardwerte ↔ `settings.xml` prüfen**

Ein Test stellt sicher, dass `settings.xml` und `Settings::Defaults()` übereinstimmen. In `tests/cpp/settings_test.cpp` anhängen:

```cpp
#include <regex>
#include <sstream>

TEST(SettingsTest, SettingsXmlMatchesDefaults)
{
  std::ifstream in(std::string(SPEKTRA_REPO_ROOT) + "/visualization.spektra/resources/settings.xml");
  ASSERT_TRUE(in);
  std::stringstream text;
  text << in.rdbuf();
  const std::string xml = text.str();
  const std::regex re(R"re(<setting id="([^"]+)"[^>]*default="([^"]+)")re");
  const auto defaults = Settings::Defaults();
  size_t seen = 0;
  for (auto it = std::sregex_iterator(xml.begin(), xml.end(), re); it != std::sregex_iterator(); ++it, ++seen)
    EXPECT_EQ((*it)[2].str(), SettingValue(defaults, (*it)[1].str())) << (*it)[1].str();
  EXPECT_EQ(seen, SettingKeys().size());
}
```

In `tests/cpp/CMakeLists.txt` nach `add_executable(core_tests …)`:

```cmake
target_compile_definitions(core_tests PRIVATE SPEKTRA_REPO_ROOT="${REPO_ROOT}")
```

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (29)

- [ ] **Step 7: Stand prüfen (nicht committen)**

---

### Task 6: Thread-sicherer Mono-Ringpuffer

**Files:**
- Create: `src/core/SampleRing.h`, `src/core/SampleRing.cpp`, `tests/cpp/sample_ring_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
namespace spektra {
class SampleRing {
public:
  explicit SampleRing(size_t capacity = 16384);
  void SetChannels(int channels);                         // < 1 wird zu 1
  void Push(const float* interleaved, size_t count);      // count = Anzahl Floats (Frames · Kanäle); thread-sicher
  bool Latest(float* destination, size_t frames) const;   // jüngste frames Mono-Samples, false wenn zu wenige; thread-sicher
  uint64_t TotalFrames() const;                           // seit Beginn geschriebene Frames; thread-sicher
};
}
```

- [ ] **Step 1: Failing test**

`tests/cpp/sample_ring_test.cpp`:

```cpp
#include "core/SampleRing.h"

#include <gtest/gtest.h>

#include <vector>

using spektra::SampleRing;

TEST(SampleRingTest, StereoIsAveraged)
{
  SampleRing ring(8);
  ring.SetChannels(2);
  const float in[] = {1.0f, 0.0f, 0.5f, 0.5f, -1.0f, 1.0f};
  ring.Push(in, 6);
  float out[3];
  ASSERT_TRUE(ring.Latest(out, 3));
  EXPECT_FLOAT_EQ(out[0], 0.5f);
  EXPECT_FLOAT_EQ(out[1], 0.5f);
  EXPECT_FLOAT_EQ(out[2], 0.0f);
  EXPECT_EQ(ring.TotalFrames(), 3u);
}

TEST(SampleRingTest, DownmixesAnyChannelCount)
{
  for (int channels : {1, 6})
  {
    SampleRing ring(16);
    ring.SetChannels(channels);
    std::vector<float> in(4 * channels);
    for (int frame = 0; frame < 4; ++frame)
      for (int c = 0; c < channels; ++c)
        in[frame * channels + c] = static_cast<float>(frame);
    ring.Push(in.data(), in.size());
    float out[4];
    ASSERT_TRUE(ring.Latest(out, 4)) << channels;
    for (int frame = 0; frame < 4; ++frame)
      EXPECT_FLOAT_EQ(out[frame], static_cast<float>(frame)) << channels;
  }
}

TEST(SampleRingTest, PartialFramesAcrossPushesStayAligned)
{
  SampleRing ring(8);
  ring.SetChannels(2);
  const float a[] = {1.0f, 1.0f, 2.0f};
  const float b[] = {2.0f, 3.0f, 3.0f};
  ring.Push(a, 3);
  ring.Push(b, 3);
  float out[3];
  ASSERT_TRUE(ring.Latest(out, 3));
  EXPECT_FLOAT_EQ(out[0], 1.0f);
  EXPECT_FLOAT_EQ(out[1], 2.0f);
  EXPECT_FLOAT_EQ(out[2], 3.0f);
}

TEST(SampleRingTest, WrapsAroundAndKeepsNewest)
{
  SampleRing ring(4);
  ring.SetChannels(1);
  const float in[] = {1, 2, 3, 4, 5, 6};
  ring.Push(in, 6);
  float out[4];
  ASSERT_TRUE(ring.Latest(out, 4));
  EXPECT_FLOAT_EQ(out[0], 3.0f);
  EXPECT_FLOAT_EQ(out[3], 6.0f);
  EXPECT_FALSE(ring.Latest(out, 5));
}

TEST(SampleRingTest, NotEnoughYet)
{
  SampleRing ring(8);
  ring.SetChannels(1);
  const float in[] = {1, 2};
  ring.Push(in, 2);
  float out[3];
  EXPECT_FALSE(ring.Latest(out, 3));
}
```

CMake: `SampleRing.cpp`, `sample_ring_test.cpp`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/SampleRing.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/core/SampleRing.h`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace spektra
{

// Mischt interleavte Kanäle zu Mono und hält die jüngsten Samples. Kodi ruft Push aus seinem
// Audio-Thread, Latest wird aus Render() gelesen.
class SampleRing
{
public:
  explicit SampleRing(size_t capacity = 16384);

  void SetChannels(int channels);
  void Push(const float* interleaved, size_t count);
  bool Latest(float* destination, size_t frames) const;
  uint64_t TotalFrames() const;

private:
  mutable std::mutex m_mutex;
  std::vector<float> m_buffer;
  size_t m_write = 0;
  uint64_t m_total = 0;
  int m_channels = 2;
  float m_partialSum = 0.0f;
  int m_partialCount = 0;
};

} // namespace spektra
```

`src/core/SampleRing.cpp`:

```cpp
#include "SampleRing.h"

#include <algorithm>

namespace spektra
{

SampleRing::SampleRing(size_t capacity) : m_buffer(std::max<size_t>(capacity, 1), 0.0f)
{
}

void SampleRing::SetChannels(int channels)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_channels = std::max(1, channels);
  m_partialSum = 0.0f;
  m_partialCount = 0;
}

void SampleRing::Push(const float* interleaved, size_t count)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  for (size_t i = 0; i < count; ++i)
  {
    m_partialSum += interleaved[i];
    if (++m_partialCount < m_channels)
      continue;
    m_buffer[m_write] = m_partialSum / static_cast<float>(m_channels);
    m_write = (m_write + 1) % m_buffer.size();
    ++m_total;
    m_partialSum = 0.0f;
    m_partialCount = 0;
  }
}

bool SampleRing::Latest(float* destination, size_t frames) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (frames > m_buffer.size() || frames > m_total)
    return false;
  size_t read = (m_write + m_buffer.size() - frames) % m_buffer.size();
  for (size_t i = 0; i < frames; ++i)
  {
    destination[i] = m_buffer[read];
    read = (read + 1) % m_buffer.size();
  }
  return true;
}

uint64_t SampleRing::TotalFrames() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_total;
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (34)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 7: Engine – Analyse-Takt, Stille, Lücken, `live.json`, Herzschlag

**Files:**
- Create: `src/core/Engine.h`, `src/core/Engine.cpp`, `tests/cpp/engine_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: `SampleRing` (Task 6), `Spectrum` (Task 3), `AudioAnalyzer`/`Features` (Task 4), `Settings`, `ApplySetting`, `ReadLiveSettings`, `WriteRendererStatus` (Task 5)
- Produces:

```cpp
namespace spektra {
struct EngineConfig {
  std::string stateDirectory;                 // addon_data/visualization.spektra (ohne Schrägstrich am Ende)
  std::function<double()> clock;              // Sekunden, monoton; leer = steady_clock
};
class Engine {
public:
  explicit Engine(EngineConfig config);
  void Start(int channels, int sampleRate);              // neue Kodi-Instanz; Analysezustand bleibt
  void PushAudio(const float* interleaved, size_t count);// thread-sicher
  const Features& Tick();                                // einmal pro Bild aus Render()
  void ApplyKodiSetting(const std::string& key, const std::string& value);
  void ReplaceSettings(const Settings& settings);        // beim Instanzstart aus Kodi gelesen
  const Settings& CurrentSettings() const;
  float LastDt() const;                                  // Simulationsschritt dieses Bildes (<= 0,1 s)
  void Heartbeat(const std::string& state, float fps, const std::string& error); // schreibt renderer.json höchstens 1×/s
};
}
```

Verhalten:
- `Tick()`: `now = clock()`; `dtReal = min(1, now − last)`; `LastDt = min(0,1, dtReal)`.
- Wurde seit > 0,1 s kein Audio geschoben → eine Stille-FFT (Nullen) analysieren, damit die Pegel abklingen.
- War die Lücke > 1 s und kommt wieder Audio → `AudioAnalyzer::Rebase()` und `Spectrum::Reset()`.
- Sonst die jüngsten 4096 Samples analysieren (`binHz = sampleRate / 4096`).
- `SetIntensity(settings.intensity / 100)` vor jeder Analyse.
- `live.json` höchstens 1×/s lesen; nur eine **höhere** `revision` als zuletzt übernommen wird angewendet (Basis: aktuelle Einstellungen).
- `Heartbeat` schreibt `stateDirectory + "/renderer.json"` höchstens 1×/s (erster Aufruf sofort).

- [ ] **Step 1: Failing test**

`tests/cpp/engine_test.cpp`:

```cpp
#include "TempDir.h"
#include "core/Engine.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace spektra;

namespace
{
struct Harness
{
  TempDir dir;
  double now = 0.0;
  Engine engine{EngineConfig{dir.Path().string(), [this] { return now; }}};
  double phase = 0.0;

  // Ein Bild bei 60 fps; optional 735 Stereo-Frames Sinus der Frequenz hz mit Amplitude amp.
  const Features& Frame(double hz, float amp = 0.5f)
  {
    now += 1.0 / 60.0;
    if (hz > 0.0)
    {
      std::vector<float> samples(735 * 2);
      for (size_t i = 0; i < 735; ++i)
      {
        const float v = amp * static_cast<float>(std::sin(phase));
        phase += 2.0 * M_PI * hz / 44100.0;
        samples[2 * i] = samples[2 * i + 1] = v;
      }
      engine.PushAudio(samples.data(), samples.size());
    }
    return engine.Tick();
  }
  void Run(double hz, double seconds, float amp = 0.5f)
  {
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i)
      Frame(hz, amp);
  }
};
} // namespace

TEST(EngineTest, ToneDrivesItsBand)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  const auto& f = h.engine.Tick();
  EXPECT_GT(f.level[kMid], 0.2f);
  EXPECT_LT(f.level[kBass], 0.05f);
}

TEST(EngineTest, SilenceAfterMusicDecays)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  h.Run(0.0, 3.0);
  for (float l : h.engine.Tick().level)
    EXPECT_LT(l, 0.05f);
}

TEST(EngineTest, StartKeepsAnalysisState)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  const float before = h.engine.Tick().level[kMid];
  ASSERT_GT(before, 0.2f);
  h.engine.Start(2, 44100); // Songwechsel: neue Kodi-Instanz
  EXPECT_NEAR(h.Frame(1000.0).level[kMid], before, 0.05f);
}

TEST(EngineTest, GapTriggersRebase)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  h.now += 5.0; // Spulen: 5 s keine Bilder und kein Audio
  h.Frame(0.0);
  // Danach deutlich leiser: ohne Neueinpegeln bliebe der Pegel klein (≈ 0,1), mit ≈ 0,4.
  h.Run(1000.0, 0.5, 0.05f);
  EXPECT_GT(h.engine.Tick().level[kMid], 0.2f);
}

TEST(EngineTest, LiveJsonIsPickedUpOncePerRevision)
{
  Harness h;
  h.engine.Start(2, 44100);
  std::ofstream(h.dir.File("live.json")) << R"({"revision": 3, "settings": {"intensity": 10}})";
  h.Run(0.0, 1.5);
  EXPECT_EQ(h.engine.CurrentSettings().intensity, 10);
  h.engine.ApplyKodiSetting("intensity", "50");
  h.Run(0.0, 1.5); // gleiche Revision: Kodi-Wert bleibt
  EXPECT_EQ(h.engine.CurrentSettings().intensity, 50);
}

TEST(EngineTest, HeartbeatIsRateLimited)
{
  Harness h;
  const auto path = h.dir.File("renderer.json");
  h.engine.Heartbeat("running", 60.0f, "");
  ASSERT_TRUE(std::filesystem::exists(path));
  std::filesystem::remove(path);
  h.now += 0.5;
  h.engine.Heartbeat("running", 60.0f, "");
  EXPECT_FALSE(std::filesystem::exists(path));
  h.now += 0.6;
  h.engine.Heartbeat("error", 0.0f, "gl_init");
  std::ifstream in(path);
  const auto json = nlohmann::json::parse(in);
  EXPECT_EQ(json["state"], "error");
  EXPECT_EQ(json["error"], "gl_init");
}

TEST(EngineTest, DtIsClampedForSimulation)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Frame(0.0);
  h.now += 2.0;
  h.engine.Tick();
  EXPECT_FLOAT_EQ(h.engine.LastDt(), 0.1f);
}
```

CMake: `Engine.cpp`, `engine_test.cpp`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`core/Engine.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/core/Engine.h`:

```cpp
#pragma once

#include "AudioAnalyzer.h"
#include "SampleRing.h"
#include "Settings.h"
#include "Spectrum.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace spektra
{

struct EngineConfig
{
  std::string stateDirectory;
  std::function<double()> clock;
};

// Einmal pro Prozess: Kodi erzeugt die Visualisierungs-Instanz bei jedem Songwechsel neu,
// Analysezustand und Einstellungen sollen das überleben.
class Engine
{
public:
  explicit Engine(EngineConfig config);

  void Start(int channels, int sampleRate);
  void PushAudio(const float* interleaved, size_t count);
  const Features& Tick();

  void ApplyKodiSetting(const std::string& key, const std::string& value);
  void ReplaceSettings(const Settings& settings);
  const Settings& CurrentSettings() const { return m_settings; }

  float LastDt() const { return m_dt; }
  void Heartbeat(const std::string& state, float fps, const std::string& error);

private:
  void CheckLiveSettings(double now);

  EngineConfig m_config;
  SampleRing m_ring;
  Spectrum m_spectrum;
  AudioAnalyzer m_analyzer;
  Settings m_settings = Settings::Defaults();
  std::vector<float> m_samples, m_silence;
  float m_sampleRate = 44100.0f;

  std::mutex m_pushMutex;
  double m_lastPush = -1e9;

  double m_lastTick = -1.0;
  double m_lastLiveCheck = -1e9;
  double m_lastHeartbeat = -1e9;
  int64_t m_liveRevision = 0;
  bool m_inGap = false;
  float m_dt = 0.0f;
};

} // namespace spektra
```

`src/core/Engine.cpp`:

```cpp
#include "Engine.h"

#include "StateFiles.h"

#include <algorithm>
#include <chrono>

namespace spektra
{
namespace
{
constexpr size_t kFftSize = 4096;
constexpr double kSilenceAfter = 0.1; // ohne Audio: Stille analysieren
constexpr double kGapRebase = 1.0;    // so lange Lücke: neu einpegeln
} // namespace

Engine::Engine(EngineConfig config) : m_config(std::move(config))
{
  if (!m_config.clock)
    m_config.clock = [] {
      return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
  m_samples.resize(kFftSize);
  m_silence.assign(kFftSize, 0.0f);
}

void Engine::Start(int channels, int sampleRate)
{
  m_ring.SetChannels(channels);
  if (sampleRate > 0)
    m_sampleRate = static_cast<float>(sampleRate);
}

void Engine::PushAudio(const float* interleaved, size_t count)
{
  m_ring.Push(interleaved, count);
  std::lock_guard<std::mutex> lock(m_pushMutex);
  m_lastPush = m_config.clock();
}

const Features& Engine::Tick()
{
  const double now = m_config.clock();
  const float dtReal = m_lastTick < 0.0 ? 1.0f / 60.0f : static_cast<float>(std::min(1.0, now - m_lastTick));
  m_lastTick = now;
  m_dt = std::min(0.1f, dtReal);

  CheckLiveSettings(now);
  m_analyzer.SetIntensity(m_settings.intensity / 100.0f);

  double lastPush;
  {
    std::lock_guard<std::mutex> lock(m_pushMutex);
    lastPush = m_lastPush;
  }
  const double silentFor = now - lastPush;
  if (silentFor > kGapRebase)
    m_inGap = true;

  const float* input = m_silence.data();
  if (silentFor <= kSilenceAfter && m_ring.Latest(m_samples.data(), kFftSize))
  {
    if (m_inGap)
    {
      m_analyzer.Rebase();
      m_spectrum.Reset();
      m_inGap = false;
    }
    input = m_samples.data();
  }
  m_spectrum.Process(input);
  return m_analyzer.Update(m_spectrum.Linear(), m_spectrum.Normalized(), m_sampleRate / kFftSize, now, dtReal);
}

void Engine::ApplyKodiSetting(const std::string& key, const std::string& value)
{
  ApplySetting(m_settings, key, value);
}

void Engine::ReplaceSettings(const Settings& settings)
{
  m_settings = settings;
}

void Engine::CheckLiveSettings(double now)
{
  if (now - m_lastLiveCheck < 1.0)
    return;
  m_lastLiveCheck = now;
  const auto live = ReadLiveSettings(m_config.stateDirectory + "/live.json", m_settings);
  if (live && live->revision > m_liveRevision)
  {
    m_liveRevision = live->revision;
    m_settings = live->settings;
  }
}

void Engine::Heartbeat(const std::string& state, float fps, const std::string& error)
{
  const double now = m_config.clock();
  if (now - m_lastHeartbeat < 1.0)
    return;
  m_lastHeartbeat = now;
  RendererStatus status;
  status.state = state;
  status.scene = m_settings.scene;
  status.fps = fps;
  status.error = error;
  WriteRendererStatus(m_config.stateDirectory + "/renderer.json", status);
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (41)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 8: Kodi-Anbindung und Diagnose-Ringe

**Files:**
- Create: `src/gl/DiagnosticRenderer.h`, `src/gl/DiagnosticRenderer.cpp`
- Modify: `src/addon.h`, `src/addon.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `Engine`, `EngineConfig`, `Features`, `SettingKeys`, `TypeOf`, `SettingType`, `Settings::Defaults`, `ApplySetting` (Tasks 4–7)
- Produces: `spektra::DiagnosticRenderer` mit `bool Init()`, `void Draw(const Features&, int width, int height)`, `const std::string& LastError() const` – wird in Plan 2 durch den Eclipse-Renderer ersetzt.

Diese Aufgabe hat keine Unit-Tests (Kodi und GL). Geprüft wird durch den Build (`./test.sh`) und die Sichtprüfung auf dem Pi durch den Maintainer.

- [ ] **Step 1: Diagnose-Renderer**

`src/gl/DiagnosticRenderer.h`:

```cpp
#pragma once

#include "core/AudioAnalyzer.h"

#include <GLES3/gl3.h>

#include <string>

namespace spektra
{

// Vier konzentrische Ringe (Bass innen … Höhen außen): Radius und Helligkeit folgen dem Pegel,
// ein Anschlag lässt den Ring kurz aufblitzen. Nur zur Sichtprüfung der Analyse auf dem Pi.
class DiagnosticRenderer
{
public:
  ~DiagnosticRenderer();

  bool Init();
  void Draw(const Features& features, int width, int height);
  const std::string& LastError() const { return m_error; }

private:
  GLuint m_program = 0;
  GLuint m_vbo = 0;
  GLint m_scale = -1;
  GLint m_color = -1;
  float m_flash[kBandCount] = {};
  std::string m_error;
};

} // namespace spektra
```

`src/gl/DiagnosticRenderer.cpp`:

```cpp
#include "DiagnosticRenderer.h"

#include <cmath>
#include <vector>

namespace spektra
{
namespace
{

constexpr int kSegments = 128;

constexpr const char* kVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
uniform vec2 uScale;
void main() { gl_Position = vec4(aPos * uScale, 0.0, 1.0); }
)";

constexpr const char* kFragment = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 outColor;
void main() { outColor = uColor; }
)";

GLuint Compile(GLenum type, const char* source, std::string& error)
{
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok)
  {
    char log[512] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    error = log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

constexpr float kColors[kBandCount][3] = {
    {0.91f, 0.64f, 0.24f}, {0.85f, 0.40f, 0.18f}, {0.95f, 0.85f, 0.65f}, {0.74f, 0.83f, 0.96f}};

} // namespace

DiagnosticRenderer::~DiagnosticRenderer()
{
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_program)
    glDeleteProgram(m_program);
}

bool DiagnosticRenderer::Init()
{
  const GLuint vs = Compile(GL_VERTEX_SHADER, kVertex, m_error);
  if (!vs)
    return false;
  const GLuint fs = Compile(GL_FRAGMENT_SHADER, kFragment, m_error);
  if (!fs)
  {
    glDeleteShader(vs);
    return false;
  }
  m_program = glCreateProgram();
  glAttachShader(m_program, vs);
  glAttachShader(m_program, fs);
  glLinkProgram(m_program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
  if (!ok)
  {
    m_error = "Shader-Programm konnte nicht gelinkt werden";
    return false;
  }
  m_scale = glGetUniformLocation(m_program, "uScale");
  m_color = glGetUniformLocation(m_program, "uColor");

  std::vector<float> circle(kSegments * 2);
  for (int i = 0; i < kSegments; ++i)
  {
    const double a = 2.0 * M_PI * i / kSegments;
    circle[2 * i] = static_cast<float>(std::cos(a));
    circle[2 * i + 1] = static_cast<float>(std::sin(a));
  }
  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, circle.size() * sizeof(float), circle.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return true;
}

void DiagnosticRenderer::Draw(const Features& features, int width, int height)
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!m_program || width <= 0 || height <= 0)
    return;
  glUseProgram(m_program);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glLineWidth(3.0f);
  const float aspect = static_cast<float>(height) / static_cast<float>(width);
  for (int b = 0; b < kBandCount; ++b)
  {
    m_flash[b] = features.onset[b] > 0.0f ? 1.0f : m_flash[b] * 0.85f;
    const float radius = 0.18f + 0.14f * b + 0.08f * features.level[b];
    const float light = 0.25f + 0.55f * features.level[b] + 0.2f * m_flash[b];
    glUniform2f(m_scale, radius * aspect, radius);
    glUniform4f(m_color, kColors[b][0] * light, kColors[b][1] * light, kColors[b][2] * light, 1.0f);
    glDrawArrays(GL_LINE_LOOP, 0, kSegments);
  }
  glDisableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
}

} // namespace spektra
```

- [ ] **Step 2: Kodi-Instanz**

`src/addon.h`:

```cpp
#pragma once

#include <kodi/addon-instance/Visualization.h>

#include <chrono>
#include <memory>
#include <string>

namespace spektra
{
class DiagnosticRenderer;
class Engine;
} // namespace spektra

// Kodi-Visualisierung Spektra. Kodi erzeugt diese Instanz bei jedem Songwechsel neu;
// Analyse und Einstellungen liegen in der prozessweiten Engine.
class ATTR_DLL_LOCAL CSpektra : public kodi::addon::CAddonBase,
                                public kodi::addon::CInstanceVisualization
{
public:
  CSpektra();
  ~CSpektra() override;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;
  bool Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName) override;
  void AudioData(const float* audioData, size_t audioDataLength) override;
  bool IsDirty() override { return true; }
  void Render() override;

private:
  spektra::Engine& m_engine;
  std::unique_ptr<spektra::DiagnosticRenderer> m_renderer; // GL-Objekte gehören zur Instanz
  bool m_rendererFailed = false;
  std::chrono::steady_clock::time_point m_fpsStart = std::chrono::steady_clock::now();
  int m_frames = 0;
  float m_fps = 0.0f;
};
```

`src/addon.cpp`:

```cpp
#include "addon.h"

#include "core/Engine.h"
#include "core/Settings.h"
#include "gl/DiagnosticRenderer.h"

namespace
{

constexpr const char* kPrefix = "[visualization.spektra] ";

std::string StateDirectory()
{
  std::string path = kodi::addon::GetUserPath();
  while (path.size() > 1 && path.back() == '/')
    path.pop_back();
  return path;
}

// Prozessweit: überlebt die Kodi-Instanzen (neue Instanz pro Song, .so bleibt geladen).
spektra::Engine& SharedEngine()
{
  static spektra::Engine engine(spektra::EngineConfig{StateDirectory(), {}});
  return engine;
}

// Alle Einstellungen typgerecht aus Kodi lesen.
spektra::Settings ReadKodiSettings()
{
  auto settings = spektra::Settings::Defaults();
  for (const auto& key : spektra::SettingKeys())
  {
    const auto type = spektra::TypeOf(key);
    if (!type)
      continue;
    std::string value;
    switch (*type)
    {
      case spektra::SettingType::Bool:
        value = kodi::addon::GetSettingBoolean(key, spektra::SettingValue(settings, key) == "true") ? "true"
                                                                                                    : "false";
        break;
      case spektra::SettingType::Int:
        value = std::to_string(kodi::addon::GetSettingInt(key, std::stoi(spektra::SettingValue(settings, key))));
        break;
      case spektra::SettingType::String:
        value = kodi::addon::GetSettingString(key, spektra::SettingValue(settings, key));
        break;
    }
    spektra::ApplySetting(settings, key, value);
  }
  return settings;
}

} // namespace

CSpektra::CSpektra() : m_engine(SharedEngine())
{
  m_engine.ReplaceSettings(ReadKodiSettings());
}

CSpektra::~CSpektra() = default;

ADDON_STATUS CSpektra::SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue)
{
  m_engine.ApplyKodiSetting(settingName, settingValue.GetString());
  return ADDON_STATUS_OK;
}

bool CSpektra::Start(int channels, int samplesPerSec, int, const std::string&)
{
  m_engine.Start(channels, samplesPerSec);
  return true;
}

void CSpektra::AudioData(const float* audioData, size_t audioDataLength)
{
  if (audioData && audioDataLength > 0)
    m_engine.PushAudio(audioData, audioDataLength);
}

void CSpektra::Render()
{
  if (!m_renderer && !m_rendererFailed)
  {
    auto renderer = std::make_unique<spektra::DiagnosticRenderer>();
    if (!renderer->Init())
    {
      m_rendererFailed = true;
      kodi::Log(ADDON_LOG_ERROR, "%sGL-Initialisierung fehlgeschlagen: %s", kPrefix, renderer->LastError().c_str());
    }
    m_renderer = std::move(renderer);
  }

  const auto& features = m_engine.Tick();
  if (m_rendererFailed)
  {
    m_engine.Heartbeat("error", 0.0f, "gl_init");
    return;
  }
  m_renderer->Draw(features, Width(), Height());

  ++m_frames;
  const auto now = std::chrono::steady_clock::now();
  const float elapsed = std::chrono::duration<float>(now - m_fpsStart).count();
  if (elapsed >= 1.0f)
  {
    m_fps = m_frames / elapsed;
    m_frames = 0;
    m_fpsStart = now;
  }
  m_engine.Heartbeat("running", m_fps, "");
}

ADDONCREATOR(CSpektra)
```

- [ ] **Step 3: CMake um alle Quellen ergänzen**

In `CMakeLists.txt` die Quell- und Headerlisten ersetzen:

```cmake
set(SPEKTRA_SOURCES src/addon.cpp
                    src/core/AudioAnalyzer.cpp
                    src/core/Engine.cpp
                    src/core/Fft.cpp
                    src/core/Routing.cpp
                    src/core/SampleRing.cpp
                    src/core/Settings.cpp
                    src/core/Spectrum.cpp
                    src/core/StateFiles.cpp
                    src/gl/DiagnosticRenderer.cpp)

set(SPEKTRA_HEADERS src/addon.h
                    src/core/AudioAnalyzer.h
                    src/core/Engine.h
                    src/core/Fft.h
                    src/core/Routing.h
                    src/core/SampleRing.h
                    src/core/Settings.h
                    src/core/Spectrum.h
                    src/core/StateFiles.h
                    src/gl/DiagnosticRenderer.h)
```

- [ ] **Step 4: Alles prüfen**

Run: `./test.sh`
Expected: `5 bestanden`, `7 bestanden`, C++ `100% tests passed` (41 Tests), Build Exit 0, Zip enthält `addon.xml`, `resources/settings.xml`, beide `strings.po` und `visualization.spektra.so.0.1.0`; `check_elf` ohne Meldung (kein `libpthread`/`libdl`: falls doch gemeldet, `Threads::Threads` nicht linken – die glibc ≥ 2.34 auf dem Pi enthält pthread).

- [ ] **Step 5: Übergabe an den Maintainer (Sichtprüfung auf dem Pi)**

Dem Maintainer melden (nicht selbst installieren):

1. `dist/visualization.spektra-0.1.0.zip` installieren, unter Player → Musik → Visualisierung „Spektra“ wählen.
2. Musik abspielen, Vollbild-Visualisierung öffnen: vier farbige Ringe; der innerste pulsiert mit dem Kick, der äußerste flackert mit Hi-Hats.
3. Pause: Ringe werden innerhalb von ~3 s dunkel und klein.
4. Einstellungen: Musik-Intensität auf 10 → Ringe reagieren deutlich schwächer.

Danach lesend prüfen:

```bash
ssh root@192.168.178.10 'cat /storage/.kodi/userdata/addon_data/visualization.spektra/renderer.json; grep "visualization.spektra" /storage/.kodi/temp/kodi.log | tail -20'
```

Expected: `{"state":"running",…,"fps":5x…}`; keine `GL-Initialisierung fehlgeschlagen`.

- [ ] **Step 6: Stand prüfen (nicht committen)**

Run: `git status --short`
