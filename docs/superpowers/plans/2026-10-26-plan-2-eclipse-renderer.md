# Spektra Plan 2: Szene Eclipse und GL-Renderer – Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Auf dem Pi läuft statt der Diagnose-Ringe die Eclipse-Szene des Prototyps `docs/entwuerfe/dune-v14-favorit.html`: schwarzes Loch, Flamme aus Pixelstaub, freier Staub im Strömungsfeld, weiche warme Corona mit Streamern und Bögen, Wellen, Glutfuß, Bloom – gesteuert über Bänder, Zuordnung, Elementregler und Pixelgröße aus Plan 1.

**Architecture:** Die Szene (`src/scene/`) ist Kodi- und GL-frei. Sie simuliert auf der CPU genau wie der Prototyp und schreibt pro Bild eine `DrawList` (Staubpunkte, weiche Corona-Punkte, Ring-/Bogen-Bänder, Bloom-Stärke, Loch). `src/gl/Renderer` zeichnet die `DrawList` mit GLES 3 in Offscreen-Texturen (Szene, weiche Corona in halber Auflösung, Bloom über zweimaliges Halbieren) und setzt das Ergebnis in Kodis Framebuffer zusammen. Die Szene ist mit GoogleTest auf amd64 prüfbar, der Renderer auf dem Pi per Screenshot.

**Tech Stack:** C++17, OpenGL ES 3.0, GoogleTest, Kodi 21.3 dev-kit; Basis ist Plan 1 (`Engine`, `Features`, `Settings`, `RoutedLevel`/`RoutedOnset`).

**Spec:** `docs/superpowers/specs/2026-10-26-spektra-design.md` (§4, §6, §6.1, §6.2)

## Global Constraints

- Verbindliche Referenz für Verhalten und Zahlen: Funktion `frame()` und Umgebung in `docs/entwuerfe/dune-v14-favorit.html`. Abweichungen nur mit Ledger-Ruling.
- Logische Bezugsgröße: Höhe 720; Breite = 720 · Seitenverhältnis des Kodi-Viewports. Gerätepixel = logisch · (Viewport-Höhe / 720).
- Farben 0–255 wie im Prototyp: WHITE 250,250,255 · SILVER 222,230,242 · GOLD 226,182,110 · EMBER 214,96,46 · RUBY 190,22,48 · AMBER 255,196,84 · BEIGE 236,214,182 · ICE 184,208,244.
- Additives Mischen wie Canvas „lighter“; Loch und weißer Rand nach dem Bloom deckend („source-over“).
- C++17, GLES 3.0 (`#version 300 es`), erlaubte Laufzeitbibliotheken unverändert (Plan 1), `-z nodelete` bleibt.
- Kerneinheiten in `src/core/` und `src/scene/` kennen weder Kodi noch GL.
- **Nicht committen.** Jede Aufgabe endet mit „Stand prüfen“.
- Pi `root@192.168.178.10`: Installation und Tests durch Claude sind vom Maintainer im Chat freigegeben (2026-10-26). Kodi-Einstellungen außer `musicplayer.visualisation` nicht ändern. Tests auf dem Pi nur mit 48-kHz-Musik (44,1 kHz knistert am Denon, siehe `CLAUDE.md`). Testdateien und Screenshots danach entfernen.

## Review Focus

1. Andere Bildschirmgröße oder Seitenverhältnis (z. B. 4K, 21:9, kleiner Vorschau-Viewport im Skin) → Ring bleibt rund und mittig, keine verzerrten oder abgeschnittenen Elemente (Task 4: `EclipseSceneTest.WideViewportKeepsRingCentered`; Task 5: Renderer rechnet nur mit Viewport-Höhe).
2. Lange Laufzeit mit sehr dichter Musik → Teilchenzahl bleibt begrenzt, Bildrate bricht nicht dauerhaft ein (Task 4: `EclipseSceneTest.ParticleLimitHolds`; Task 5: fps in `renderer.json` bei 100 % Intensität).
3. Kodi-GL-Zustand → nach unserem `Render()` sind Framebuffer, Viewport, Blend, Scissor, Programm, VAO, Puffer und Textur wiederhergestellt; sonst zeichnet Kodi sein OSD falsch (Task 5: Sichtprüfung OSD über der Visualisierung).
4. Pixelgröße sehr klein auf großem Schirm bzw. sehr groß → kein Verschwinden, keine Überhelligkeit (Task 1: `PixelGridTest`; Task 5: Screenshots 0,5× und 8×).
5. Musik bricht mitten im Drop ab (Pause) → Szene beruhigt sich zum glimmenden Ring statt einzufrieren (Task 4: `EclipseSceneTest.SilenceLeavesAGlimmeringRing`).

## Dateistruktur

| Datei | Verantwortung |
|---|---|
| `src/scene/DrawList.h` | Zeichenliste: `Dust`, `Annulus`, `DrawList`, Helfer `MakeDust`, `Line`, `Arc` |
| `src/scene/Random.h` | schneller deterministischer Zufall (`Uniform`, `Gauss`, `SpawnCount`) |
| `src/scene/Scene.h` | Schnittstelle aller Szenen |
| `src/scene/PixelGrid.h` | Pixelgröße → Blockraster, Alpha-Ausgleich, Helligkeitsausgleich |
| `src/scene/eclipse/Palette.h` | Farben, `Mix`, `FireColor` (64 Stufen) |
| `src/scene/eclipse/Context.h` | Werte eines Bildes (Zeit, Mitte, R, Regler, Strömung), `Flow`, `AngleDistance`, `kTau` |
| `src/scene/eclipse/DustPool.{h,cpp}` | Flammen- und freier Staub: Bewegung, Entfernen, Zeichnen, Staubausbruch |
| `src/scene/eclipse/Flame.{h,cpp}` | 240 Flammenzellen: Grundglut, Puls-Buckel, Hitze, Emission |
| `src/scene/eclipse/Corona.{h,cpp}` | Saum, 9 Streamer, Bögen, Corona-Staub |
| `src/scene/eclipse/EclipseScene.{h,cpp}` | Zusammenspiel, Wellen, Zuordnung, Drop, Kernringe, Glutfuß, Loch |
| `src/gl/Renderer.{h,cpp}` | GLES-3-Ausgabe der `DrawList` |
| `src/addon.{h,cpp}` | Szene + Renderer statt Diagnose-Ringe |
| entfällt: `src/gl/DiagnosticRenderer.{h,cpp}` | |
| Tests: `tests/cpp/scene_basics_test.cpp`, `flame_dust_test.cpp`, `corona_test.cpp`, `eclipse_scene_test.cpp` | |

---

### Task 1: Szenen-Grundlagen

**Files:**
- Create: `src/scene/DrawList.h`, `src/scene/Random.h`, `src/scene/Scene.h`, `src/scene/PixelGrid.h`, `src/scene/eclipse/Palette.h`, `src/scene/eclipse/Context.h`, `tests/cpp/scene_basics_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt` (Test eintragen)

**Interfaces:**
- Consumes: `spektra::Features`, `spektra::Settings` (Plan 1)
- Produces:

```cpp
namespace spektra {
struct Color { float r, g, b, a; };                        // 0..1, Alpha nicht vormultipliziert
struct Dust { float x, y, size, r, g, b; };               // logisch; rgb = Farbe/255 · Alpha (additiv)
struct Annulus { float cx, cy, r0, r1, angle0, angle1; Color inner, outer; bool hairline; };
struct DrawList { float width, height, pixel, pixelGain, bloom, holeX, holeY, holeRadius;
                  std::vector<Annulus> under, over; std::vector<Dust> soft, dust; Annulus holeRing; void Clear(); };
Dust MakeDust(float x, float y, int size, const Rgb& c, float alpha);
Annulus Line(float cx, float cy, float radius, float width, const Rgb& c, float alpha);           // Vollkreis-Strich
Annulus Arc(float cx, float cy, float radius, float width, float a0, float a1, const Rgb& c, float alpha);
class Random { explicit Random(uint64_t seed); float Uniform(); float Gauss(); int SpawnCount(float rate); };
class Scene { virtual void SetViewport(float w, float h) = 0;
              virtual void Frame(const Features&, const Settings&, float dt, DrawList& out) = 0; };
struct PixelGrid { int grid; float alpha; };
PixelGrid ComputePixelGrid(float pixel, float deviceScale);
float PixelGain(float pixel);
struct Rgb { float r, g, b; };   // 0..255
Rgb Mix(const Rgb&, const Rgb&, float); Rgb FireColor(float h);
constexpr float kTau, kPi;
struct Context { float t, dt, cx, cy, R, width, height, M, flame, waves, particles, corona, flowT, flowStrength, shimT, shimAmp; int maxParticles; };
void Flow(float u, float v, float t, float& fx, float& fy);
float AngleDistance(float a, float b);
}
```

- [ ] **Step 1: Failing test**

`tests/cpp/scene_basics_test.cpp`:

```cpp
#include "scene/DrawList.h"
#include "scene/PixelGrid.h"
#include "scene/Random.h"
#include "scene/eclipse/Context.h"
#include "scene/eclipse/Palette.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

TEST(PaletteTest, FireColorUsesPrototypeSteps)
{
  const Rgb white = FireColor(0.0f);
  EXPECT_FLOAT_EQ(white.r, 250.0f);
  EXPECT_FLOAT_EQ(white.b, 255.0f);
  // Stufe 63: 0,999 · 4 = 3,996 -> Glut nach Rubin, Anteil 0,996
  const Rgb top = FireColor(1.0f);
  EXPECT_NEAR(top.r, 214.0f + (190.0f - 214.0f) * 0.996f, 0.01f);
  // Stufe 31: 31/63 · 4 = 1,968 -> Silber nach Gold, Anteil 0,968
  const Rgb mid = FireColor(0.5f);
  EXPECT_NEAR(mid.g, 230.0f + (182.0f - 230.0f) * (31.0f / 63.0f * 4.0f - 1.0f), 0.01f);
}

TEST(RandomTest, SameSeedSameSequence)
{
  Random a(7), b(7);
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(a.Uniform(), b.Uniform());
}

TEST(RandomTest, UniformStaysInRange)
{
  Random r(3);
  double sum = 0.0;
  for (int i = 0; i < 100000; ++i)
  {
    const float v = r.Uniform();
    ASSERT_GE(v, 0.0f);
    ASSERT_LT(v, 1.0f);
    sum += v;
  }
  EXPECT_NEAR(sum / 100000.0, 0.5, 0.01);
}

TEST(RandomTest, SpawnCountAveragesTheRate)
{
  Random r(5);
  long total = 0;
  for (int i = 0; i < 100000; ++i)
    total += r.SpawnCount(2.3f);
  EXPECT_NEAR(total / 100000.0, 2.3, 0.02);
  EXPECT_EQ(r.SpawnCount(0.0f), 0);
}

TEST(FlowTest, FieldIsDivergenceFree)
{
  const float h = 1e-3f;
  for (float u : {-1.5f, 0.2f, 2.0f})
    for (float v : {-0.7f, 0.9f})
    {
      float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
      Flow(u + h, v, 1.3f, fx1, fy1);
      Flow(u - h, v, 1.3f, fx2, fy2);
      Flow(u, v + h, 1.3f, fx3, fy3);
      Flow(u, v - h, 1.3f, fx4, fy4);
      const float divergence = (fx1 - fx2) / (2 * h) + (fy3 - fy4) / (2 * h);
      EXPECT_NEAR(divergence, 0.0f, 0.05f) << u << "/" << v;
    }
}

TEST(AngleTest, DistanceWrapsAround)
{
  EXPECT_NEAR(AngleDistance(0.1f, kTau - 0.1f), 0.2f, 1e-4f);
  EXPECT_NEAR(AngleDistance(kPi, 0.0f), kPi, 1e-4f);
}

TEST(PixelGridTest, BlocksAndFineDust)
{
  const auto normal = ComputePixelGrid(1.0f, 1.5f);
  EXPECT_EQ(normal.grid, 2);
  EXPECT_FLOAT_EQ(normal.alpha, 1.0f);
  const auto fine = ComputePixelGrid(0.5f, 1.5f);
  EXPECT_EQ(fine.grid, 1);
  EXPECT_FLOAT_EQ(fine.alpha, 0.5625f);
  EXPECT_EQ(ComputePixelGrid(8.0f, 1.5f).grid, 12);
  EXPECT_FLOAT_EQ(PixelGain(1.0f), 1.0f);
  EXPECT_FLOAT_EQ(PixelGain(2.0f), std::pow(2.0f, -1.1f));
  EXPECT_FLOAT_EQ(PixelGain(0.5f), std::pow(0.5f, -0.8f));
}

TEST(DrawListTest, DustIsPremultiplied)
{
  const Dust d = MakeDust(1.0f, 2.0f, 2, Rgb{255.0f, 127.5f, 0.0f}, 0.5f);
  EXPECT_FLOAT_EQ(d.r, 0.5f);
  EXPECT_FLOAT_EQ(d.g, 0.25f);
  EXPECT_FLOAT_EQ(d.b, 0.0f);
  EXPECT_FLOAT_EQ(d.size, 2.0f);
  const Annulus l = Line(10.0f, 20.0f, 100.0f, 2.0f, Rgb{255.0f, 255.0f, 255.0f}, 0.4f);
  EXPECT_FLOAT_EQ(l.r0, 99.0f);
  EXPECT_FLOAT_EQ(l.r1, 101.0f);
  EXPECT_TRUE(l.hairline);
  EXPECT_FLOAT_EQ(l.inner.a, 0.4f);
}
```

In `tests/cpp/CMakeLists.txt` `scene_basics_test.cpp` zu `TEST_SOURCES`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`scene/DrawList.h: No such file or directory`)

- [ ] **Step 3: Implementierung**

`src/scene/eclipse/Palette.h`:

```cpp
#pragma once

#include <algorithm>

namespace spektra
{

// Farben wie im Prototyp, Kanäle 0..255.
struct Rgb
{
  float r, g, b;
};

constexpr Rgb kWhite{250.0f, 250.0f, 255.0f};
constexpr Rgb kSilver{222.0f, 230.0f, 242.0f};
constexpr Rgb kGold{226.0f, 182.0f, 110.0f};
constexpr Rgb kEmber{214.0f, 96.0f, 46.0f};
constexpr Rgb kRuby{190.0f, 22.0f, 48.0f};
constexpr Rgb kAmber{255.0f, 196.0f, 84.0f};
constexpr Rgb kBeige{236.0f, 214.0f, 182.0f};
constexpr Rgb kIce{184.0f, 208.0f, 244.0f};

inline Rgb Mix(const Rgb& a, const Rgb& b, float k)
{
  return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k};
}

// Flammenfarbe weiß -> silber -> gold -> glut -> rubin, in 64 Stufen wie die Tabelle des Prototyps.
inline Rgb FireColor(float h)
{
  static const Rgb stops[5] = {kWhite, kSilver, kGold, kEmber, kRuby};
  const int step = static_cast<int>(std::min(0.999f, std::max(0.0f, h)) * 63.0f);
  const float x = std::min(0.999f, static_cast<float>(step) / 63.0f) * 4.0f;
  const int i = static_cast<int>(x);
  return Mix(stops[i], stops[i + 1], x - static_cast<float>(i));
}

} // namespace spektra
```

`src/scene/DrawList.h`:

```cpp
#pragma once

#include "eclipse/Palette.h"

#include <vector>

namespace spektra
{

// Farbe 0..1 mit nicht vormultipliziertem Alpha (für Bänder).
struct Color
{
  float r, g, b, a;
};

// Ein Staubkorn in logischen Koordinaten; rgb = Farbe/255 · Alpha, additiv gezeichnet.
struct Dust
{
  float x, y, size, r, g, b;
};

// Kreisring-Ausschnitt von r0 bis r1 zwischen angle0 und angle1 (Bogenmaß), Farbe innen -> außen.
// hairline: die Breite ist eine Strichstärke; unter 1 Gerätepixel zeichnet der Renderer 1 Pixel
// mit entsprechend geringerer Deckkraft (wie Canvas).
struct Annulus
{
  float cx, cy, r0, r1, angle0, angle1;
  Color inner, outer;
  bool hairline;
};

struct DrawList
{
  float width = 1280.0f, height = 720.0f;
  float pixel = 1.0f, pixelGain = 1.0f;
  std::vector<Annulus> under;  // additiv, vor dem Staub (Wellen)
  std::vector<Dust> soft;   // additiv, halbe Auflösung, weich (Corona)
  std::vector<Dust> dust;   // additiv, scharf, Blockraster
  std::vector<Annulus> over;   // additiv, nach dem Staub (Kernringe, Glutfuß)
  float bloom = 0.5f;
  float holeX = 640.0f, holeY = 360.0f, holeRadius = 0.0f; // deckend schwarz nach dem Bloom
  Annulus holeRing{};                                          // deckend nach dem Loch

  void Clear()
  {
    under.clear();
    soft.clear();
    dust.clear();
    over.clear();
  }
};

inline Dust MakeDust(float x, float y, int size, const Rgb& c, float alpha)
{
  const float k = alpha / 255.0f;
  return {x, y, static_cast<float>(size), c.r * k, c.g * k, c.b * k};
}

inline Annulus Arc(float cx, float cy, float radius, float width, float a0, float a1, const Rgb& c, float alpha)
{
  const Color color{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha};
  return {cx, cy, radius - width / 2.0f, radius + width / 2.0f, a0, a1, color, color, true};
}

inline Annulus Line(float cx, float cy, float radius, float width, const Rgb& c, float alpha)
{
  return Arc(cx, cy, radius, width, 0.0f, 6.283185307f, c, alpha);
}

} // namespace spektra
```

`src/scene/Random.h`:

```cpp
#pragma once

#include <cmath>
#include <cstdint>

namespace spektra
{

// Xorshift64: schnell, deterministisch je Samen (Tests), gut genug für Teilcheneffekte.
class Random
{
public:
  explicit Random(uint64_t seed = 0x9E3779B97F4A7C15ull) : m_state(seed ? seed : 1) {}

  // Gleichverteilt in [0, 1).
  float Uniform()
  {
    m_state ^= m_state << 13;
    m_state ^= m_state >> 7;
    m_state ^= m_state << 17;
    return static_cast<float>(m_state >> 40) * (1.0f / 16777216.0f);
  }

  // Wie im Prototyp: Summe dreier Gleichverteilungen, auf ±1 skaliert.
  float Gauss() { return (Uniform() + Uniform() + Uniform() - 1.5f) / 1.5f; }

  // floor(rate) plus 1 mit der Wahrscheinlichkeit des Nachkommaanteils.
  int SpawnCount(float rate)
  {
    if (rate <= 0.0f)
      return 0;
    const int n = static_cast<int>(std::floor(rate));
    return n + (Uniform() < rate - static_cast<float>(n) ? 1 : 0);
  }

private:
  uint64_t m_state;
};

} // namespace spektra
```

`src/scene/Scene.h`:

```cpp
#pragma once

#include "DrawList.h"
#include "core/AudioAnalyzer.h"
#include "core/Settings.h"

namespace spektra
{

// Eine Szene simuliert ein Bild und beschreibt es als DrawList. Kein Kodi, kein GL.
class Scene
{
public:
  virtual ~Scene() = default;
  // Logische Größe: Höhe 720, Breite nach Seitenverhältnis.
  virtual void SetViewport(float width, float height) = 0;
  virtual void Frame(const Features& features, const Settings& settings, float dt, DrawList& out) = 0;
};

} // namespace spektra
```

`src/scene/PixelGrid.h`:

```cpp
#pragma once

#include <algorithm>
#include <cmath>

namespace spektra
{

// Pixelgröße P (logische Pixel) bei deviceScale Gerätepixeln je logischem Pixel:
// Blockkante k = P · deviceScale. k >= 1: Raster round(k); k < 1: 1 Gerätepixel mit Alpha · k².
struct PixelGrid
{
  int grid;
  float alpha;
};

inline PixelGrid ComputePixelGrid(float pixel, float deviceScale)
{
  const float k = pixel * deviceScale;
  if (k < 1.0f)
    return {1, k * k};
  return {std::max(1, static_cast<int>(std::lround(k))), 1.0f};
}

// Helligkeitsausgleich wie im Prototyp: große Blöcke dunkler, feiner Staub heller.
inline float PixelGain(float pixel)
{
  return std::pow(pixel, pixel > 1.0f ? -1.1f : -0.8f);
}

} // namespace spektra
```

`src/scene/eclipse/Context.h`:

```cpp
#pragma once

#include <cmath>

namespace spektra
{

constexpr float kPi = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

// Werte eines Bildes, die alle Teile der Eclipse-Szene brauchen.
struct Context
{
  float t = 0.0f, dt = 1.0f / 60.0f;
  float cx = 640.0f, cy = 360.0f, R = 136.8f, width = 1280.0f, height = 720.0f;
  float M = 0.7f;                                                   // Musik-Intensität
  float flame = 1.0f, waves = 1.0f, particles = 1.0f, corona = 1.0f; // Elementregler 0..1
  float flowT = 0.0f, flowStrength = 0.0f, shimT = 0.0f, shimAmp = 0.15f;
  int maxParticles = 42000;
};

// Gemeinsames, divergenzfreies Strömungsfeld (Curl einer Stromfunktion), wie im Prototyp.
inline void Flow(float u, float v, float t, float& fx, float& fy)
{
  const float a1 = u * 1.7f + t * 0.35f, b1 = v * 1.3f - t * 0.27f;
  const float a2 = (u * 0.9f - v * 1.1f) * 1.6f + t * 0.5f;
  const float a3 = u * 3.1f + t * 0.8f, b3 = v * 2.7f - t * 0.6f;
  const float dpu = 1.7f * std::cos(a1) * std::cos(b1) + 0.6f * 1.44f * std::cos(a2) +
                    0.35f * 3.1f * std::cos(a3) * std::sin(b3);
  const float dpv = -1.3f * std::sin(a1) * std::sin(b1) - 0.6f * 1.76f * std::cos(a2) +
                    0.35f * 2.7f * std::sin(a3) * std::cos(b3);
  fx = dpv;
  fy = -dpu;
}

// Kleinster Abstand zweier Winkel, 0..pi.
inline float AngleDistance(float a, float b)
{
  float d = std::fmod(a - b + kPi, kTau);
  if (d < 0.0f)
    d += kTau;
  return std::fabs(d - kPi);
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (43 + 9 = 52)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 2: Flamme und Staub

**Files:**
- Create: `src/scene/eclipse/DustPool.h`, `src/scene/eclipse/DustPool.cpp`, `src/scene/eclipse/Flame.h`, `src/scene/eclipse/Flame.cpp`, `tests/cpp/flame_dust_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt` (Quellen `DustPool.cpp`, `Flame.cpp`; Test)

**Interfaces:**
- Consumes: Task 1 (`Context`, `Flow`, `AngleDistance`, `Random`, `DrawList`, `MakeDust`, `FireColor`), `kFineBands` (Plan 1)
- Produces:

```cpp
namespace spektra {
struct Grain { float x, y, vx, vy, life, max; bool free; int size; };
class DustPool {
public:
  explicit DustPool(Random& rng);
  size_t Size() const;
  void Add(const Grain& g);
  void Release(const Context& c, float strength, int spots);   // Staubausbruch
  void Step(const Context& c, DrawList& out);                   // bewegen, entfernen, zeichnen
  const std::vector<Grain>& Grains() const;
};
class Flame {
public:
  static constexpr int kCells = 240;
  explicit Flame(Random& rng);
  void PulseProfile(float strength, int nMin = 3, int nMax = 7, float wMax = 0.5f, float floor = 0.15f);
  void Decay(float dt);
  void Emit(const Context& c, float flameLevel, float freeLevel, float loudness,
            const std::array<float, kFineBands>& fine, DustPool& pool);
  float Heat(int cell) const;
  float HeatAt(float angle) const;
};
}
```

- [ ] **Step 1: Failing test**

`tests/cpp/flame_dust_test.cpp`:

```cpp
#include "scene/eclipse/DustPool.h"
#include "scene/eclipse/Flame.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
std::array<float, kFineBands> Fine(float v)
{
  std::array<float, kFineBands> f{};
  f.fill(v);
  return f;
}
} // namespace

TEST(FlameTest, EmitsAllAroundTheRing)
{
  Random rng(1);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  for (int i = 0; i < 30; ++i)
  {
    c.t += c.dt;
    flame.Decay(c.dt);
    flame.Emit(c, 0.6f, 0.0f, 0.5f, Fine(0.4f), pool);
  }
  int quadrant[4] = {};
  for (const auto& g : pool.Grains())
    ++quadrant[(g.x >= c.cx ? 1 : 0) + (g.y >= c.cy ? 2 : 0)];
  for (int q : quadrant)
    EXPECT_GT(q, 20);
}

TEST(FlameTest, FlameRegulatorZeroEmitsNothing)
{
  Random rng(1);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  c.flame = 0.0f;
  flame.PulseProfile(1.5f);
  for (int i = 0; i < 30; ++i)
    flame.Emit(c, 1.0f, 1.0f, 1.0f, Fine(1.0f), pool);
  EXPECT_EQ(pool.Size(), 0u);
}

TEST(FlameTest, PulseRaisesHeat)
{
  Random rng(9);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  flame.Emit(c, 0.0f, 0.0f, 0.0f, Fine(0.0f), pool);
  float before = 0.0f;
  for (int i = 0; i < Flame::kCells; ++i)
    before += flame.Heat(i);
  flame.PulseProfile(1.0f);
  flame.Emit(c, 0.0f, 0.0f, 0.0f, Fine(0.0f), pool);
  float after = 0.0f;
  for (int i = 0; i < Flame::kCells; ++i)
    after += flame.Heat(i);
  EXPECT_GT(after, before * 3.0f);
  EXPECT_FLOAT_EQ(flame.HeatAt(0.0f), flame.Heat(0));
  EXPECT_FLOAT_EQ(flame.HeatAt(kPi), flame.Heat(Flame::kCells / 2));
}

TEST(FlameTest, EmissionThinsOutAtTheLimit)
{
  Random rng(4);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  c.maxParticles = 500;
  flame.PulseProfile(1.5f);
  for (int i = 0; i < 120; ++i)
    flame.Emit(c, 1.0f, 1.0f, 1.0f, Fine(1.0f), pool);
  EXPECT_LE(pool.Size(), 500u);
}

TEST(DustPoolTest, NothingStaysInsideTheRing)
{
  Random rng(2);
  DustPool pool(rng);
  Context c;
  pool.Add({c.cx + c.R * 0.5f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  pool.Add({c.cx + c.R * 1.2f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  DrawList out;
  pool.Step(c, out);
  EXPECT_EQ(pool.Size(), 1u);
  EXPECT_EQ(out.dust.size(), 1u);
}

TEST(DustPoolTest, RegulatorsRemoveTheirGrainsAtOnce)
{
  Random rng(2);
  DustPool pool(rng);
  Context c;
  pool.Add({c.cx + c.R * 1.5f, c.cy, 0.0f, 0.0f, 3.0f, 3.0f, true, 1});
  pool.Add({c.cx + c.R * 1.2f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  c.particles = 0.0f;
  DrawList out;
  pool.Step(c, out);
  ASSERT_EQ(pool.Size(), 1u);
  EXPECT_FALSE(pool.Grains()[0].free);
  c.flame = 0.0f;
  out.Clear();
  pool.Step(c, out);
  EXPECT_EQ(pool.Size(), 0u);
  EXPECT_TRUE(out.dust.empty());
}

TEST(DustPoolTest, ReleaseRespectsTheLimit)
{
  Random rng(6);
  DustPool pool(rng);
  Context c;
  c.maxParticles = 100;
  pool.Release(c, 1.0f, 6);
  EXPECT_EQ(pool.Size(), 100u);
  for (const auto& g : pool.Grains())
    EXPECT_TRUE(g.free);
}

TEST(DustPoolTest, ReleaseDoesNothingWithoutParticles)
{
  Random rng(6);
  DustPool pool(rng);
  Context c;
  c.particles = 0.0f;
  pool.Release(c, 1.0f, 6);
  EXPECT_EQ(pool.Size(), 0u);
}
```

CMake: `${REPO_ROOT}/src/scene/eclipse/DustPool.cpp`, `${REPO_ROOT}/src/scene/eclipse/Flame.cpp` zu `CORE_SOURCES`, `flame_dust_test.cpp` zu `TEST_SOURCES`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`DustPool.cpp`/`DustPool.h` fehlen)

- [ ] **Step 3: Implementierung**

`src/scene/eclipse/DustPool.h`:

```cpp
#pragma once

#include "Context.h"
#include "scene/DrawList.h"
#include "scene/Random.h"

#include <cstddef>
#include <vector>

namespace spektra
{

// Ein Korn der Flamme (gebunden) oder losgelöster Staub (frei).
struct Grain
{
  float x, y, vx, vy, life, max;
  bool free;
  int size;
};

// Flammen- und freier Staub: Bewegung im Strömungsfeld, Entfernen, Zeichnen (Prototyp: px[]).
class DustPool
{
public:
  explicit DustPool(Random& rng) : m_rng(rng) {}

  size_t Size() const { return m_grains.size(); }
  const std::vector<Grain>& Grains() const { return m_grains; }
  void Add(const Grain& grain) { m_grains.push_back(grain); }

  // Staubausbruch: an `spots` Stellen reißt Staub gleichzeitig ab (Höhen-Anschlag, Drop).
  void Release(const Context& c, float strength, int spots);

  // Bewegt alle Körner, entfernt verbrauchte (und die eines Reglers auf 0) und zeichnet die übrigen.
  void Step(const Context& c, DrawList& out);

private:
  Random& m_rng;
  std::vector<Grain> m_grains;
};

} // namespace spektra
```

`src/scene/eclipse/DustPool.cpp`:

```cpp
#include "DustPool.h"

#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

void DustPool::Release(const Context& c, float strength, int spots)
{
  if (c.particles <= 0.0f)
    return;
  for (int s = 0; s < spots; ++s)
  {
    const float a0 = m_rng.Uniform() * kTau;
    const float n = std::floor(60.0f + 260.0f * strength) * c.particles;
    for (int j = 0; static_cast<float>(j) < n && static_cast<int>(m_grains.size()) < c.maxParticles; ++j)
    {
      const float a = a0 + m_rng.Gauss() * 0.08f, r0 = c.R * 1.01f;
      const float speed = c.R * (0.4f + m_rng.Uniform() * 0.9f) * (0.6f + strength);
      const float life = 2.0f + m_rng.Uniform() * 3.0f;
      m_grains.push_back({c.cx + std::cos(a) * r0, c.cy + std::sin(a) * r0, std::cos(a) * speed,
                          std::sin(a) * speed, life, life, true, m_rng.Uniform() < 0.9f ? 1 : 2});
    }
  }
}

void DustPool::Step(const Context& c, DrawList& out)
{
  const float dragBound = std::pow(0.986f, c.dt * 60.0f);
  // Rückwärts mit Tausch ans Ende: wie im Prototyp, der getauschte Eintrag ist schon bearbeitet.
  for (size_t i = m_grains.size(); i-- > 0;)
  {
    Grain& p = m_grains[i];
    p.life -= c.dt;
    const float dx = p.x - c.cx, dy = p.y - c.cy;
    float d = std::hypot(dx, dy);
    if (d == 0.0f)
      d = 1.0f;
    const float nx = dx / d, ny = dy / d;
    const float regulator = p.free ? c.particles : c.flame;
    if (p.life <= 0.0f || d < c.R || p.x < 0.0f || p.y < 0.0f || p.x >= c.width || p.y >= c.height ||
        regulator == 0.0f)
    {
      m_grains[i] = m_grains.back();
      m_grains.pop_back();
      continue;
    }
    const float u = dx / c.R, v = dy / c.R;
    float fx, fy;
    Flow(u, v, c.flowT, fx, fy);
    if (p.free)
    {
      // freie Körner folgen der Strömung und driften langsam nach außen
      const float tx = fx * c.flowStrength + nx * c.R * 0.18f, ty = fy * c.flowStrength + ny * c.R * 0.18f;
      p.vx += (tx - p.vx) * 1.6f * c.dt;
      p.vy += (ty - p.vy) * 1.6f * c.dt;
    }
    else
    {
      // gebundene: Auftrieb nach außen, Sog zurück, seitlich von der Strömung geschwenkt
      p.vx += fx * c.flowStrength * 1.2f * c.dt - nx * c.R * 0.45f * c.dt;
      p.vy += fy * c.flowStrength * 1.2f * c.dt - ny * c.R * 0.45f * c.dt;
      p.vx *= dragBound;
      p.vy *= dragBound;
    }
    p.x += p.vx * c.dt;
    p.y += p.vy * c.dt;
    const float k = 1.0f - p.life / p.max;
    const float h = p.free ? 0.35f + k * 0.6f : std::min(1.0f, (d - c.R) / (c.R * 0.9f)) * 0.75f + k * 0.25f;
    const float shimmer = 1.0f - c.shimAmp + c.shimAmp * std::sin(u * 5.0f + v * 3.5f - c.shimT * 2.4f);
    const float alpha =
        (p.free ? std::min(1.0f, p.life) * 0.55f * c.particles : (1.0f - k * k) * 0.75f * c.flame) * shimmer;
    out.dust.push_back(MakeDust(p.x, p.y, p.size, FireColor(h), alpha));
  }
}

} // namespace spektra
```

`src/scene/eclipse/Flame.h`:

```cpp
#pragma once

#include "Context.h"
#include "DustPool.h"
#include "core/AudioAnalyzer.h"
#include "scene/Random.h"

#include <array>

namespace spektra
{

// Durchgehende Flamme: 240 Zellen mit Grundglut, langsamen Wellen, Feinspektrum und Puls-Buckeln.
class Flame
{
public:
  static constexpr int kCells = 240;

  explicit Flame(Random& rng);

  // Zufälliges, buckliges Profil rund um den Ring; Zellen nehmen das Maximum aus alt und neu.
  void PulseProfile(float strength, int nMin = 3, int nMax = 7, float wMax = 0.5f, float floor = 0.15f);
  // Buckel klingen ab, Grundglut wandert zufällig.
  void Decay(float dt);
  // Berechnet die Hitze jeder Zelle und gibt Körner an den Staub ab.
  void Emit(const Context& c, float flameLevel, float freeLevel, float loudness,
            const std::array<float, kFineBands>& fine, DustPool& pool);

  float Heat(int cell) const { return m_heat[static_cast<size_t>(cell)]; }
  float HeatAt(float angle) const;

private:
  Random& m_rng;
  std::array<float, kCells> m_boost{}, m_ember{}, m_heat{}, m_phase{};
};

} // namespace spektra
```

`src/scene/eclipse/Flame.cpp`:

```cpp
#include "Flame.h"

#include <algorithm>
#include <cmath>

namespace spektra
{
namespace
{

// Unregelmäßige, langsam wandernde Zuordnung Winkel -> Stelle im Feinspektrum.
float FreqAt(float a, float t)
{
  return 0.5f + 0.5f * std::sin(a * 3.0f + t * 0.07f) * std::cos(a * 5.0f - t * 0.05f + 1.3f);
}

float SampleFine(const std::array<float, kFineBands>& fine, float f)
{
  f = std::min(1.0f, std::max(0.0f, f)) * static_cast<float>(kFineBands - 1);
  const int i = static_cast<int>(f);
  const float k = f - static_cast<float>(i);
  return fine[static_cast<size_t>(i)] * (1.0f - k) + fine[static_cast<size_t>(std::min(kFineBands - 1, i + 1))] * k;
}

} // namespace

Flame::Flame(Random& rng) : m_rng(rng)
{
  for (int i = 0; i < kCells; ++i)
  {
    m_phase[static_cast<size_t>(i)] = m_rng.Uniform() * kTau;
    m_ember[static_cast<size_t>(i)] = m_rng.Uniform();
  }
}

void Flame::PulseProfile(float strength, int nMin, int nMax, float wMax, float floor)
{
  struct Bump
  {
    float a, w, h;
  } bumps[16];
  const int n = std::min(16, nMin + static_cast<int>(m_rng.Uniform() * static_cast<float>(nMax - nMin + 1)));
  for (int k = 0; k < n; ++k)
    bumps[k] = {m_rng.Uniform() * kTau, 0.06f + m_rng.Uniform() * wMax, 0.4f + m_rng.Uniform() * 0.9f};
  for (int i = 0; i < kCells; ++i)
  {
    const float a = static_cast<float>(i) * kTau / kCells;
    float v = floor * (0.6f + m_rng.Uniform());
    for (int k = 0; k < n; ++k)
    {
      const float d = AngleDistance(a, bumps[k].a);
      v += bumps[k].h * std::exp(-(d * d) / (2.0f * bumps[k].w * bumps[k].w));
    }
    auto& boost = m_boost[static_cast<size_t>(i)];
    boost = std::max(boost, v * strength);
  }
}

void Flame::Decay(float dt)
{
  const float decay = std::exp(-dt * 2.2f);
  for (int i = 0; i < kCells; ++i)
  {
    m_boost[static_cast<size_t>(i)] *= decay;
    auto& ember = m_ember[static_cast<size_t>(i)];
    ember = std::min(1.0f, std::max(0.0f, ember + (m_rng.Uniform() - 0.5f) * dt * 2.5f));
  }
}

void Flame::Emit(const Context& c, float flameLevel, float freeLevel, float loudness,
                 const std::array<float, kFineBands>& fine, DustPool& pool)
{
  const float lowDrive = 0.18f + 1.2f * flameLevel;
  for (int i = 0; i < kCells; ++i)
  {
    const auto at = [this](int cell) { return m_ember[static_cast<size_t>((cell + kCells) % kCells)]; };
    const float a = static_cast<float>(i) * kTau / kCells;
    const float n1 =
        0.5f + 0.5f * std::sin(a * 7.0f + c.t * 0.9f + m_phase[static_cast<size_t>(i)] * 0.2f) * std::sin(a * 3.0f - c.t * 0.6f);
    const float n2 = 0.5f + 0.5f * std::sin(a * 13.0f - c.t * 1.7f + 2.0f);
    const float base =
        lowDrive * (0.12f + 0.22f * n1 * n1 + 0.1f * n2 + 0.12f * (at(i - 1) + at(i) + at(i + 1)) / 3.0f);
    const float spec = SampleFine(fine, FreqAt(a, c.t)) * (0.2f + 0.3f * loudness);
    const float power = std::min(1.4f, base + spec + m_boost[static_cast<size_t>(i)]) * c.flame;
    m_heat[static_cast<size_t>(i)] = power;

    // Dichte gleichmäßig ausdünnen, wenn viele Körner unterwegs sind (keine Stelle verhungert).
    const float room = std::max(0.0f, 1.0f - static_cast<float>(pool.Size()) / static_cast<float>(c.maxParticles));
    const int n = m_rng.SpawnCount(240.0f * std::pow(power, 1.5f) * c.dt * (1.0f + loudness * 0.5f) * room);
    // mehr Pegel für „Staub löst sich“ -> mehr freie Körner
    const float freeP = (0.04f + 0.25f * std::max(0.0f, power - 0.4f)) * (0.3f + 1.4f * freeLevel) * c.particles;
    for (int j = 0; j < n; ++j)
    {
      const float aa = a + (m_rng.Uniform() - 0.5f) * (kTau / kCells) * 1.5f;
      const float speed = c.R * (0.3f + m_rng.Uniform() * 1.4f) * power;
      const bool free = m_rng.Uniform() < freeP;
      const float life = free ? 2.5f + m_rng.Uniform() * 3.0f : 0.5f + m_rng.Uniform() * 0.9f * std::min(1.0f, power);
      const float r0 = c.R * (1.0f + m_rng.Uniform() * 0.01f);
      const float push = free ? 1.2f : 1.0f;
      pool.Add({c.cx + std::cos(aa) * r0, c.cy + std::sin(aa) * r0, std::cos(aa) * speed * push,
                std::sin(aa) * speed * push, life, life, free, m_rng.Uniform() < 0.88f ? 1 : 2});
    }
  }
}

float Flame::HeatAt(float angle) const
{
  float f = std::fmod(angle / kTau, 1.0f);
  if (f < 0.0f)
    f += 1.0f;
  const int cell = std::min(kCells - 1, static_cast<int>(f * kCells));
  return m_heat[static_cast<size_t>(cell)];
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (52 + 8 = 60)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 3: Corona

**Files:**
- Create: `src/scene/eclipse/Corona.h`, `src/scene/eclipse/Corona.cpp`, `tests/cpp/corona_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1, `Flame::HeatAt` (Task 2)
- Produces:

```cpp
namespace spektra {
class Corona {
public:
  explicit Corona(Random& rng);
  void Strike(float angle, float strength);                 // Streamer an der Stelle der Tonhöhe, 45 % mit Bogen
  void Drop(float corona);                                  // Corona voll, alle Streamer neu
  void Step(const Context& c, float hold, const Flame& flame, float birth, DrawList& out);
  size_t Size() const;
  float Level() const;
};
}
```

Abweichung vom Prototyp (Ruling im Ledger beim Umsetzen): Die Zahl der Corona-Teilchen wird auf `maxParticles` begrenzt (der Prototyp hatte keine Grenze); ein Bogen-Teilchen trägt die Daten seines Bogens selbst (der Prototyp hielt einen Verweis auf das Bogen-Objekt).

- [ ] **Step 1: Failing test**

`tests/cpp/corona_test.cpp`:

```cpp
#include "scene/eclipse/Corona.h"
#include "scene/eclipse/Flame.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
struct Fixture
{
  Random rng{11};
  Flame flame{rng};
  Corona corona{rng};
  Context c;
  DrawList out;

  void Run(float hold, float seconds)
  {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i)
    {
      c.t += c.dt;
      out.Clear();
      corona.Step(c, hold, flame, 0.0f, out);
    }
  }
};
} // namespace

TEST(CoronaTest, ComesWithMelodyAndFadesWithoutIt)
{
  Fixture f;
  f.corona.Strike(0.0f, 1.0f);
  f.Run(0.8f, 2.0f);
  const size_t peak = f.out.soft.size();
  EXPECT_GT(peak, 500u);
  EXPECT_GT(f.corona.Level(), 0.5f);
  f.Run(0.0f, 15.0f);
  EXPECT_LT(f.corona.Level(), 0.1f);
  EXPECT_LT(f.out.soft.size(), peak / 4);
}

TEST(CoronaTest, RegulatorZeroClearsEverything)
{
  Fixture f;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 1.0f);
  ASSERT_GT(f.corona.Size(), 0u);
  f.c.corona = 0.0f;
  f.Run(1.0f, 0.1f);
  EXPECT_EQ(f.corona.Size(), 0u);
  EXPECT_TRUE(f.out.soft.empty());
  EXPECT_TRUE(f.out.dust.empty());
}

TEST(CoronaTest, StrikeGrowsAStreamerWhereTheMelodyIs)
{
  Fixture f;
  f.corona.Strike(kPi / 2.0f, 1.0f); // unten (y wächst nach unten)
  f.Run(0.5f, 1.5f);
  int below = 0, above = 0;
  for (const auto& d : f.out.soft)
  {
    if (d.y > f.c.cy + f.c.R * 1.5f)
      ++below;
    if (d.y < f.c.cy - f.c.R * 1.5f)
      ++above;
  }
  EXPECT_GT(below, 4 * (above + 1));
}

TEST(CoronaTest, LimitHolds)
{
  Fixture f;
  f.c.maxParticles = 500;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 2.0f);
  EXPECT_LE(f.corona.Size(), 500u);
}

TEST(CoronaTest, NothingInsideTheHole)
{
  Fixture f;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 2.0f);
  for (const auto& d : f.out.soft)
    EXPECT_GE(std::hypot(d.x - f.c.cx, d.y - f.c.cy), f.c.R * 0.95f);
}
```

CMake: `Corona.cpp`, `corona_test.cpp`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`Corona.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/scene/eclipse/Corona.h`:

```cpp
#pragma once

#include "Context.h"
#include "Flame.h"
#include "scene/DrawList.h"
#include "scene/Random.h"

#include <array>
#include <cstddef>
#include <vector>

namespace spektra
{

// Warme Corona aus Staub: goldener Saum, 9 Streamer, Magnetbögen. Kommt mit der Melodie, geht ohne sie.
class Corona
{
public:
  explicit Corona(Random& rng);

  void Strike(float angle, float strength);
  void Drop(float corona);
  void Step(const Context& c, float hold, const Flame& flame, float birth, DrawList& out);

  size_t Size() const { return m_particles.size(); }
  float Level() const { return m_level; }

private:
  struct Streamer
  {
    float a = 0.0f, w = 0.0f, warm = 0.0f, len = 0.0f, curve = 0.0f, lvl = 0.0f;
  };
  struct Loop
  {
    float a, span, h, age, life;
  };
  enum Kind : int
  {
    kRim = 0,
    kStreamer = 1,
    kLoop = 2
  };
  struct Particle
  {
    int kind;
    float x, y, vx, vy, life, max, b;
    float warm, curve;                  // Streamer
    float loopA, loopSpan, loopH, strand, off; // Bogen
  };

  void Reseed(Streamer& s, float angle);
  bool Room(const Context& c) const { return static_cast<int>(m_particles.size()) < c.maxParticles; }

  Random& m_rng;
  std::array<Streamer, 9> m_streamers{};
  std::vector<Loop> m_loops;
  std::vector<Particle> m_particles;
  float m_level = 0.0f;
};

} // namespace spektra
```

`src/scene/eclipse/Corona.cpp`:

```cpp
#include "Corona.h"

#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

Corona::Corona(Random& rng) : m_rng(rng)
{
  for (auto& s : m_streamers)
    Reseed(s, m_rng.Uniform() * kTau);
}

void Corona::Reseed(Streamer& s, float angle)
{
  s.a = angle;
  s.w = 0.04f + m_rng.Uniform() * 0.16f;
  s.warm = m_rng.Uniform();
  s.len = 1.3f + m_rng.Uniform() * 2.6f;
  s.curve = (m_rng.Uniform() - 0.5f) * 0.25f;
}

void Corona::Strike(float angle, float strength)
{
  // schwächsten Streamer an der Stelle der Tonhöhe neu einsetzen
  auto weakest = std::min_element(m_streamers.begin(), m_streamers.end(),
                                  [](const Streamer& a, const Streamer& b) { return a.lvl < b.lvl; });
  Reseed(*weakest, angle + m_rng.Gauss() * 0.15f);
  weakest->lvl = std::min(1.4f, 0.5f + strength);
  if (m_rng.Uniform() < 0.45f)
    m_loops.push_back({angle + m_rng.Gauss() * 0.2f, 0.07f + m_rng.Uniform() * 0.2f,
                       0.08f + m_rng.Uniform() * 0.22f, 0.0f, 2.5f + m_rng.Uniform() * 3.0f});
}

void Corona::Drop(float corona)
{
  m_level = corona;
  for (auto& s : m_streamers)
  {
    Reseed(s, m_rng.Uniform() * kTau);
    s.lvl = 1.2f * corona;
  }
}

void Corona::Step(const Context& c, float hold, const Flame& flame, float birth, DrawList& out)
{
  const float target = (0.04f * c.M + 0.9f * hold) * c.corona;
  m_level += (target - m_level) * (1.0f - std::exp(-c.dt / (hold > m_level ? 0.6f : 2.5f)));
  if (c.corona == 0.0f)
    m_level = 0.0f;
  for (auto& s : m_streamers)
  {
    s.lvl *= std::exp(-c.dt / (1.2f + 4.0f * hold));
    s.a += s.curve * 0.02f * c.dt;
  }

  // Saum am Rand: dicht, golden, ungleichmäßig (stärker unter aktiven Streamern)
  int n = m_rng.SpawnCount((300.0f * c.corona * c.M + 5600.0f * m_level) * c.dt);
  for (int j = 0; j < n && Room(c); ++j)
  {
    const Streamer& s = m_streamers[static_cast<size_t>(m_rng.Uniform() * m_streamers.size())];
    const float a = m_rng.Uniform() < 0.55f ? s.a + m_rng.Gauss() * s.w * 3.0f : m_rng.Uniform() * kTau;
    const float r = c.R * (1.004f + std::pow(m_rng.Uniform(), 3.0f) * 0.14f);
    const float life = 0.5f + m_rng.Uniform();
    Particle p{};
    p.kind = kRim;
    p.x = c.cx + std::cos(a) * r;
    p.y = c.cy + std::sin(a) * r;
    p.vx = -std::sin(a) * c.R * m_rng.Gauss() * 0.05f;
    p.vy = std::cos(a) * c.R * m_rng.Gauss() * 0.05f;
    p.life = p.max = life;
    p.b = 0.3f + 0.7f * std::min(1.0f, m_level + s.lvl * 0.5f);
    m_particles.push_back(p);
  }
  // Streamer: Fasern ziehen weit nach außen
  for (const auto& s : m_streamers)
  {
    const float vis = s.lvl * (0.3f + 0.7f * m_level) * c.corona;
    if (vis < 0.02f)
      continue;
    n = m_rng.SpawnCount(1500.0f * vis * c.dt);
    for (int j = 0; j < n && Room(c); ++j)
    {
      const float a = s.a + m_rng.Gauss() * s.w, r = c.R * 1.01f;
      const float speed = c.R * (0.35f + m_rng.Uniform() * 0.7f);
      const float life = s.len * (0.5f + m_rng.Uniform() * 0.7f) * c.R / speed;
      Particle p{};
      p.kind = kStreamer;
      p.x = c.cx + std::cos(a) * r;
      p.y = c.cy + std::sin(a) * r;
      p.vx = std::cos(a) * speed;
      p.vy = std::sin(a) * speed;
      p.life = p.max = life;
      p.b = vis;
      p.warm = s.warm;
      p.curve = s.curve;
      m_particles.push_back(p);
    }
  }
  // Magnetbögen: Staub läuft vom Rand über einen Bogen zurück zum Rand
  for (size_t i = m_loops.size(); i-- > 0;)
  {
    Loop& lp = m_loops[i];
    lp.age += c.dt;
    if (lp.age > lp.life)
    {
      m_loops.erase(m_loops.begin() + static_cast<long>(i));
      continue;
    }
    const float env = std::sin(lp.age / lp.life * kPi);
    n = m_rng.SpawnCount(700.0f * env * c.dt);
    for (int j = 0; j < n && Room(c); ++j)
    {
      Particle p{};
      p.kind = kLoop;
      p.life = p.max = 1.2f + m_rng.Uniform() * 1.2f;
      p.b = env;
      p.loopA = lp.a;
      p.loopSpan = lp.span;
      p.loopH = lp.h;
      p.strand = 0.75f + m_rng.Uniform() * 0.5f;
      p.off = m_rng.Gauss() * 0.01f;
      m_particles.push_back(p);
    }
  }

  if (c.corona == 0.0f)
  {
    m_particles.clear();
    m_loops.clear();
  }
  const float birthGlow = 1.0f + 0.5f * birth;
  const auto soft = [&](float x, float y, const Rgb& col, float al) {
    out.soft.push_back(MakeDust(x, y, 1, col, al * c.corona));
  };
  const auto sharp = [&](float x, float y, const Rgb& col, float al) {
    out.dust.push_back(MakeDust(x, y, 1, col, al * c.corona));
  };
  for (size_t i = m_particles.size(); i-- > 0;)
  {
    Particle& p = m_particles[i];
    p.life -= c.dt;
    if (p.life <= 0.0f)
    {
      m_particles[i] = m_particles.back();
      m_particles.pop_back();
      continue;
    }
    const float k = 1.0f - p.life / p.max;
    if (p.kind == kLoop)
    {
      const float a = p.loopA - p.loopSpan / 2.0f + p.loopSpan * k + p.off;
      const float r = c.R * (1.0f + p.loopH * p.strand * std::sin(kPi * k));
      p.x = c.cx + std::cos(a) * r;
      p.y = c.cy + std::sin(a) * r;
      const float al = (0.5f * p.b * std::sin(kPi * k) + 0.1f) * birthGlow;
      soft(p.x, p.y, kAmber, al * 0.6f);
      sharp(p.x, p.y, kAmber, al * 0.5f);
      continue;
    }
    const float dx = p.x - c.cx, dy = p.y - c.cy;
    float d = std::hypot(dx, dy);
    if (d == 0.0f)
      d = 1.0f;
    if (d < c.R)
    {
      m_particles[i] = m_particles.back();
      m_particles.pop_back();
      continue;
    }
    const float nx = dx / d, ny = dy / d, u = dx / c.R, v = dy / c.R;
    float fx, fy;
    Flow(u, v, c.flowT, fx, fy);
    const float heat = flame.HeatAt(std::atan2(dy, dx));
    if (p.kind == kStreamer)
    {
      // radial, leicht gebogen, von Strömung und Flammenhitze geschwenkt
      const float push = c.R * 0.3f * std::max(0.0f, heat - 0.5f);
      p.vx += (fx * c.flowStrength * 0.25f - ny * p.curve * c.R * 0.3f) * c.dt + nx * push * c.dt;
      p.vy += (fy * c.flowStrength * 0.25f + nx * p.curve * c.R * 0.3f) * c.dt + ny * push * c.dt;
    }
    else
    {
      p.vx += fx * c.flowStrength * 0.15f * c.dt;
      p.vy += fy * c.flowStrength * 0.15f * c.dt;
    }
    p.x += p.vx * c.dt;
    p.y += p.vy * c.dt;
    const float rr = d / c.R - 1.0f;
    if (p.kind == kRim)
    {
      const float al = 0.55f * p.b * std::sin(kPi * k) * std::exp(-rr * 9.0f) * birthGlow;
      soft(p.x, p.y, kAmber, al);
      sharp(p.x, p.y, kAmber, al * 0.35f);
    }
    else
    {
      const float fade = std::sin(kPi * std::min(1.0f, k * 1.15f)) * std::exp(-rr * 0.55f);
      const float far = std::min(1.0f, rr / 2.2f);
      const Rgb col = far < 0.35f ? Mix(kAmber, kBeige, far / 0.35f)
                                  : Mix(kBeige, p.warm > 0.5f ? kBeige : kIce, (far - 0.35f) / 0.65f);
      const float al = 0.16f * p.b * fade;
      soft(p.x, p.y, col, al);
      if (m_rng.Uniform() < 0.15f)
        sharp(p.x, p.y, col, al * 0.8f);
    }
  }
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (60 + 5 = 65)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 4: EclipseScene – Zusammenspiel, Wellen, Zuordnung, Drop

**Files:**
- Create: `src/scene/eclipse/EclipseScene.h`, `src/scene/eclipse/EclipseScene.cpp`, `tests/cpp/eclipse_scene_test.cpp`
- Modify: `tests/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1–3, `RoutedLevel`, `RoutedOnset`, `kPixelSteps`, Effekte `kBreath…kDust` (Plan 1)
- Produces:

```cpp
namespace spektra {
class EclipseScene : public Scene {
public:
  explicit EclipseScene(uint64_t seed = 0x5EC7A11ull);
  void SetViewport(float width, float height) override;
  void Frame(const Features& features, const Settings& settings, float dt, DrawList& out) override;
  size_t DustCount() const; size_t CoronaCount() const; size_t WaveCount() const;
};
}
```

- [ ] **Step 1: Failing test**

`tests/cpp/eclipse_scene_test.cpp`:

```cpp
#include "scene/eclipse/EclipseScene.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
Features Loud()
{
  Features f;
  f.level = {0.7f, 0.7f, 0.7f, 0.7f};
  f.loudness = 0.6f;
  f.centroid = 0.5f;
  f.fine.fill(0.5f);
  return f;
}

struct Run
{
  EclipseScene scene{42};
  Settings settings = Settings::Defaults();
  DrawList out;

  // n Bilder; alle `every` Bilder (0 = nie) schlagen die Bänder in `bands` an.
  void Frames(int n, const Features& base, int every = 0, std::initializer_list<int> bands = {kBass, kLow, kMid, kHigh})
  {
    for (int i = 0; i < n; ++i)
    {
      Features f = base;
      if (every > 0 && i % every == 0)
        for (int b : bands)
          f.onset[static_cast<size_t>(b)] = 0.8f;
      scene.Frame(f, settings, 1.0f / 60.0f, out);
    }
  }
};
} // namespace

TEST(EclipseSceneTest, RingSitsInTheCenter)
{
  Run r;
  r.scene.SetViewport(1280.0f, 720.0f);
  r.Frames(1, Features{});
  EXPECT_FLOAT_EQ(r.out.holeX, 640.0f);
  EXPECT_FLOAT_EQ(r.out.holeY, 360.0f);
  EXPECT_NEAR(r.out.holeRadius, 0.19f * 720.0f * 0.992f, 0.03f * 0.19f * 720.0f);
}

TEST(EclipseSceneTest, WideViewportKeepsRingCentered)
{
  Run r;
  r.scene.SetViewport(1680.0f, 720.0f); // 21:9
  r.Frames(60, Loud(), 20);
  EXPECT_FLOAT_EQ(r.out.holeX, 840.0f);
  EXPECT_FLOAT_EQ(r.out.width, 1680.0f);
  EXPECT_NEAR(r.out.holeRadius, 0.19f * 720.0f * 0.992f, 0.06f * 0.19f * 720.0f);
}

TEST(EclipseSceneTest, NoDustInsideTheHole)
{
  Run r;
  r.Frames(300, Loud(), 20);
  const float R = r.out.holeRadius / 0.992f;
  ASSERT_FALSE(r.out.dust.empty());
  for (const auto& d : r.out.dust)
    ASSERT_GE(std::hypot(d.x - r.out.holeX, d.y - r.out.holeY), R * 0.9f);
  for (const auto& d : r.out.soft)
    ASSERT_GE(std::hypot(d.x - r.out.holeX, d.y - r.out.holeY), R * 0.9f);
}

TEST(EclipseSceneTest, ElementsAtZeroDrawNothing)
{
  Run r;
  r.settings.flame = r.settings.particles = r.settings.corona = r.settings.waves = 0;
  r.Frames(120, Loud(), 20);
  EXPECT_TRUE(r.out.dust.empty());
  EXPECT_TRUE(r.out.soft.empty());
  EXPECT_TRUE(r.out.under.empty());
  EXPECT_EQ(r.scene.WaveCount(), 0u);
}

TEST(EclipseSceneTest, SilenceLeavesAGlimmeringRing)
{
  Run r;
  r.Frames(300, Loud(), 20);
  r.Frames(600, Features{});
  EXPECT_GT(r.scene.DustCount(), 0u);
  EXPECT_LT(r.scene.DustCount(), 4000u);
  EXPECT_LT(r.scene.CoronaCount(), 1500u);
  EXPECT_TRUE(r.out.under.empty());
  EXPECT_EQ(r.out.over.front().inner.a, 0.95f); // weißer Kernring bleibt
}

TEST(EclipseSceneTest, WaveFollowsTheRouting)
{
  Run r;
  r.settings.route[kWave] = {false, false, true, false};
  r.Frames(1, Features{}, 1, {kBass});
  EXPECT_EQ(r.scene.WaveCount(), 0u);
  r.Frames(1, Features{}, 1, {kMid});
  EXPECT_EQ(r.scene.WaveCount(), 1u);
}

TEST(EclipseSceneTest, DropFiresDoubleWaveOnlyWhenEnabled)
{
  Features f;
  f.drop = true;
  Run on;
  on.Frames(1, f);
  EXPECT_EQ(on.scene.WaveCount(), 2u);
  Run off;
  off.settings.drop = false;
  off.Frames(1, f);
  EXPECT_EQ(off.scene.WaveCount(), 0u);
}

TEST(EclipseSceneTest, ParticleLimitHolds)
{
  Run r;
  r.settings.maxParticles = 10000;
  r.settings.intensity = 100;
  r.Frames(600, Loud(), 5);
  EXPECT_LE(r.scene.DustCount(), 10000u);
  EXPECT_LE(r.scene.CoronaCount(), 10000u);
}

TEST(EclipseSceneTest, SameSeedSameResult)
{
  Run a, b;
  a.Frames(200, Loud(), 15);
  b.Frames(200, Loud(), 15);
  EXPECT_EQ(a.scene.DustCount(), b.scene.DustCount());
  EXPECT_EQ(a.scene.CoronaCount(), b.scene.CoronaCount());
}

TEST(EclipseSceneTest, PixelSettingReachesDrawList)
{
  Run r;
  r.settings.pixel = 6;
  r.Frames(1, Features{});
  EXPECT_FLOAT_EQ(r.out.pixel, 4.0f);
  EXPECT_FLOAT_EQ(r.out.pixelGain, std::pow(4.0f, -1.1f));
}
```

CMake: `EclipseScene.cpp`, `eclipse_scene_test.cpp`.

- [ ] **Step 2: Run test to verify it fails**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: FAIL beim Bauen (`EclipseScene.h` fehlt)

- [ ] **Step 3: Implementierung**

`src/scene/eclipse/EclipseScene.h`:

```cpp
#pragma once

#include "Corona.h"
#include "DustPool.h"
#include "Flame.h"
#include "scene/Random.h"
#include "scene/Scene.h"

#include <cstdint>
#include <vector>

namespace spektra
{

// Digitale Sonnenfinsternis (Prototyp dune-v14): schwarzes Loch, Flamme aus Pixelstaub,
// freier Staub, warme Corona, Wellen. Jedes Element hängt über die Zuordnung an einem Band.
class EclipseScene : public Scene
{
public:
  explicit EclipseScene(uint64_t seed = 0x5EC7A11ull);

  void SetViewport(float width, float height) override;
  void Frame(const Features& features, const Settings& settings, float dt, DrawList& out) override;

  size_t DustCount() const { return m_dust.Size(); }
  size_t CoronaCount() const { return m_corona.Size(); }
  size_t WaveCount() const { return m_waves.size(); }

private:
  struct Wave
  {
    float t0, s;
  };

  // Tonhöhe -> Stelle am Ring: tief unten, hoch oben, zufällig links oder rechts.
  float MelodyAngle(float centroid);

  Random m_rng;
  DustPool m_dust;
  Flame m_flame;
  Corona m_corona;
  std::vector<Wave> m_waves;
  float m_width = 1280.0f, m_height = 720.0f;
  float m_t = 0.0f, m_beatT = -9.0f, m_flowT = 0.0f, m_shimT = 0.0f;
};

} // namespace spektra
```

`src/scene/eclipse/EclipseScene.cpp`:

```cpp
#include "EclipseScene.h"

#include "Palette.h"
#include "core/Routing.h"
#include "scene/PixelGrid.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

EclipseScene::EclipseScene(uint64_t seed) : m_rng(seed), m_dust(m_rng), m_flame(m_rng), m_corona(m_rng)
{
}

void EclipseScene::SetViewport(float width, float height)
{
  if (width > 0.0f && height > 0.0f)
  {
    m_width = width;
    m_height = height;
  }
}

float EclipseScene::MelodyAngle(float centroid)
{
  const float a = kPi / 2.0f - centroid * kPi;
  return m_rng.Uniform() < 0.5f ? a : kPi - a;
}

void EclipseScene::Frame(const Features& f, const Settings& s, float dt, DrawList& out)
{
  out.Clear();
  out.width = m_width;
  out.height = m_height;
  m_t += dt;

  Context c;
  c.t = m_t;
  c.dt = dt;
  c.width = m_width;
  c.height = m_height;
  c.cx = m_width / 2.0f;
  c.cy = m_height / 2.0f;
  c.M = static_cast<float>(s.intensity) / 100.0f;
  c.flame = static_cast<float>(s.flame) / 100.0f;
  c.waves = static_cast<float>(s.waves) / 100.0f;
  c.particles = static_cast<float>(s.particles) / 100.0f;
  c.corona = static_cast<float>(s.corona) / 100.0f;
  c.maxParticles = s.maxParticles;

  const float loud = f.loudness;
  const Rgb tint = Mix(kSilver, kGold, 0.15f + 0.35f * loud);
  const float onWave = RoutedOnset(s, f, kWave), onBurst = RoutedOnset(s, f, kBurst);
  const float onJet = RoutedOnset(s, f, kJet), onStreamer = RoutedOnset(s, f, kStreamer);
  const float onDust = RoutedOnset(s, f, kDust);
  const bool drop = s.drop && f.drop;

  // Anschläge: Welle, Flammenstoß rundum
  if (onWave > 0.05f)
  {
    m_beatT = m_t;
    if (c.waves > 0.0f)
      m_waves.push_back({m_t, std::max(0.2f, onWave) * c.waves});
  }
  if (onBurst > 0.05f)
  {
    m_beatT = m_t;
    m_flame.PulseProfile(0.45f + 0.8f * onBurst);
  }
  const float birth = std::exp(-(m_t - m_beatT) * 3.5f);
  c.R = m_height * 0.19f *
        (1.0f + 0.05f * RoutedLevel(s, f, kBreath) + 0.035f * birth * c.waves + 0.012f * std::sin(m_t * kTau / 9.0f));

  // Stichflammen
  if (onJet > 0.05f)
    m_flame.PulseProfile(0.35f + 0.6f * onJet, 1, 2, 0.12f, 0.0f);

  // Drop: alles zugleich
  if (drop)
  {
    m_flame.PulseProfile(1.5f, 6, 9, 0.6f, 0.4f);
    if (c.waves > 0.0f)
    {
      m_waves.push_back({m_t, c.waves});
      m_waves.push_back({m_t + 0.25f, 0.8f * c.waves});
    }
    m_corona.Drop(c.corona);
    m_dust.Release(c, 1.0f, 6);
  }
  while (!m_waves.empty() && m_t - m_waves.front().t0 > 7.0f)
    m_waves.erase(m_waves.begin());

  // Flamme: Grundhöhe aus dem Pegel, Unregelmäßigkeit aus dem Feinspektrum
  m_flame.Decay(dt);
  m_flame.Emit(c, RoutedLevel(s, f, kFlame), RoutedLevel(s, f, kFree), loud, f.fine, m_dust);
  if (onDust > 0.05f)
    m_dust.Release(c, onDust, 1 + static_cast<int>(onDust * 3.0f));

  // Wellen: anfangs über einen Lichtschleier mit dem Kern verbunden
  for (const auto& w : m_waves)
  {
    const float age = m_t - w.t0;
    if (age < 0.0f)
      continue;
    const float k = age / 7.0f, rr = c.R * (1.0f + age * 0.2f + age * age * 0.025f);
    const float link = std::max(0.0f, 1.0f - age / 1.6f);
    if (link > 0.0f)
    {
      const Color inner{tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, 0.09f * link * w.s * c.waves};
      Color outer = inner;
      outer.a = 0.015f * link * w.s * c.waves;
      out.under.push_back({c.cx, c.cy, c.R, rr, 0.0f, kTau, inner, outer, false});
    }
    out.under.push_back(Line(c.cx, c.cy, rr, 1.0f - 0.5f * k, tint, 0.45f * w.s * c.waves * (1.0f - k) * (1.0f - k)));
  }

  // Strömung und Flimmern
  const float flowLevel = RoutedLevel(s, f, kFlow);
  m_flowT += dt * (0.4f + 1.6f * flowLevel);
  m_shimT += dt * (1.0f + 4.0f * flowLevel);
  c.flowT = m_flowT;
  c.shimT = m_shimT;
  c.shimAmp = 0.15f + 0.45f * flowLevel;
  c.flowStrength = c.R * (0.3f + 0.55f * flowLevel);
  m_dust.Step(c, out);

  // Corona: Pegel hält sie am Leben, Anschläge setzen Streamer an die Stelle der Tonhöhe
  if (onStreamer > 0.05f && c.corona > 0.0f)
    m_corona.Strike(MelodyAngle(f.centroid), onStreamer);
  m_corona.Step(c, RoutedLevel(s, f, kCorona), m_flame, birth, out);

  // Kernringe und Glutfuß
  out.over.push_back(Line(c.cx, c.cy, c.R, 1.8f, kWhite, 0.95f));
  out.over.push_back(Line(c.cx, c.cy, c.R * (1.014f + 0.006f * std::sin(m_t * 0.5f + 1.0f)), 0.6f, tint, 0.22f));
  for (int i = 0; i < Flame::kCells; ++i)
  {
    const float e = std::max(0.0f, m_flame.Heat(i) - 0.35f);
    if (e < 0.02f)
      continue;
    const float a0 = static_cast<float>(i) / Flame::kCells * kTau;
    const float a1 = (static_cast<float>(i) + 1.2f) / Flame::kCells * kTau;
    Annulus arc = Arc(c.cx, c.cy, c.R, 3.0f, a0, a1, kGold, std::min(0.7f, e * 0.8f));
    arc.hairline = false;
    out.over.push_back(arc);
  }

  out.bloom = 0.5f + 0.35f * loud;
  out.holeX = c.cx;
  out.holeY = c.cy;
  out.holeRadius = c.R * 0.992f;
  out.holeRing = Line(c.cx, c.cy, c.R, 1.8f, kWhite, 0.95f);
  out.pixel = kPixelSteps[std::min(8, std::max(0, s.pixel))];
  out.pixelGain = PixelGain(out.pixel);
}

} // namespace spektra
```

- [ ] **Step 4: Run test to verify it passes**

Run: `scripts/in-container.sh scripts/test-cpp-in-container.sh`
Expected: alle Tests grün (65 + 10 = 75)

- [ ] **Step 5: Stand prüfen (nicht committen)**

---

### Task 5: GL-Renderer, Kodi-Anbindung, Prüfung auf dem Pi

**Files:**
- Create: `src/gl/Renderer.h`, `src/gl/Renderer.cpp`
- Modify: `src/addon.h`, `src/addon.cpp`, `CMakeLists.txt`
- Delete: `src/gl/DiagnosticRenderer.h`, `src/gl/DiagnosticRenderer.cpp`

**Interfaces:**
- Consumes: `DrawList`, `Annulus`, `Dust`, `ComputePixelGrid` (Task 1), `EclipseScene` (Task 4), `Engine` (Plan 1)
- Produces: `spektra::Renderer` mit `bool Init()`, `void Draw(const DrawList&)`, `const std::string& LastError() const`.

Ablauf je Bild (Spec §4.1, Prototyp `frame()`):
1. Weiche Corona (`soft`) als 1-Pixel-Punkte in eine Textur halber Auflösung, additiv.
2. Szenen-Textur volle Auflösung: schwarz löschen; `under` additiv (`SRC_ALPHA, ONE`); weiche Textur linear vergrößert additiv; `dust` als Punkte additiv (`ONE, ONE`) auf Blockraster; `over` additiv.
3. Bloom: Szene → halbe → viertel Auflösung (linear, ohne Mischen = Mittelwert 4×4).
4. Kodis Framebuffer: Szene kopieren, Viertel-Textur · `bloom` additiv dazu, dann Loch (schwarzer Kreis, deckend) und `holeRing` (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`).
5. Kodis GL-Zustand wiederherstellen: Framebuffer, Viewport, Scissor, Blend (an/aus und Faktoren), Programm, VAO, Array-Buffer, aktive Textur und deren Bindung, Cull/Depth.

Keine Unit-Tests (GL); geprüft wird durch Build und auf dem Pi per Screenshot.

- [ ] **Step 1: Renderer**

`src/gl/Renderer.h`:

```cpp
#pragma once

#include "scene/DrawList.h"

#include <GLES3/gl3.h>

#include <string>
#include <vector>

namespace spektra
{

// Zeichnet eine DrawList mit GLES 3 in den aktuell gebundenen Framebuffer und Viewport (Kodi).
// GL-Objekte gehören zur Kodi-Instanz.
class Renderer
{
public:
  ~Renderer();

  bool Init();
  void Draw(const DrawList& list);
  const std::string& LastError() const { return m_error; }

private:
  struct Target
  {
    GLuint fbo = 0, texture = 0;
    int width = 0, height = 0;
  };

  bool Ensure(Target& target, int width, int height);
  void Free(Target& target);
  void DrawPoints(int targetWidth, int targetHeight);
  void DrawBands(int targetWidth, int targetHeight);
  void DrawTexture(GLuint texture, float gain);
  void AppendBand(const Annulus& band, float scale);

  GLuint m_pointProgram = 0, m_bandProgram = 0, m_quadProgram = 0;
  GLint m_pointTarget = -1, m_bandTarget = -1, m_quadTexture = -1, m_quadGain = -1;
  GLuint m_vao = 0, m_vbo = 0;
  float m_maxPointSize = 64.0f;
  Target m_scene, m_soft, m_half, m_quarter;
  std::vector<float> m_points, m_bands;
  std::string m_error;
};

} // namespace spektra
```

`src/gl/Renderer.cpp`:

```cpp
#include "Renderer.h"

#include "scene/PixelGrid.h"

#include <algorithm>
#include <cmath>

namespace spektra
{
namespace
{

constexpr const char* kPointVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec3 aColor;
uniform vec2 uTarget;
out vec3 vColor;
void main() {
  gl_Position = vec4(aPos.x / uTarget.x * 2.0 - 1.0, 1.0 - aPos.y / uTarget.y * 2.0, 0.0, 1.0);
  gl_PointSize = aSize;
  vColor = aColor;
}
)";

constexpr const char* kPointFragment = R"(#version 300 es
precision highp float;
in vec3 vColor;
out vec4 outColor;
void main() { outColor = vec4(vColor, 0.0); }
)";

constexpr const char* kBandVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uTarget;
out vec4 vColor;
void main() {
  gl_Position = vec4(aPos.x / uTarget.x * 2.0 - 1.0, 1.0 - aPos.y / uTarget.y * 2.0, 0.0, 1.0);
  vColor = aColor;
}
)";

constexpr const char* kBandFragment = R"(#version 300 es
precision highp float;
in vec4 vColor;
out vec4 outColor;
void main() { outColor = vColor; }
)";

constexpr const char* kQuadVertex = R"(#version 300 es
out vec2 vUv;
void main() {
  vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
  vUv = pos[gl_VertexID] * 0.5 + 0.5;
}
)";

constexpr const char* kQuadFragment = R"(#version 300 es
precision highp float;
uniform sampler2D uTexture;
uniform float uGain;
in vec2 vUv;
out vec4 outColor;
void main() { outColor = vec4(texture(uTexture, vUv).rgb * uGain, 1.0); }
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
    char log[1024] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    error = log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint Link(const char* vertex, const char* fragment, std::string& error)
{
  const GLuint vs = Compile(GL_VERTEX_SHADER, vertex, error);
  if (!vs)
    return 0;
  const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment, error);
  if (!fs)
  {
    glDeleteShader(vs);
    return 0;
  }
  const GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
  {
    char log[1024] = {};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    error = log;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

// Kodis GL-Zustand, der beim Zeichnen verändert und danach wiederhergestellt wird.
struct SavedState
{
  GLint framebuffer = 0, viewport[4] = {}, program = 0, vao = 0, arrayBuffer = 0, activeTexture = 0, texture = 0;
  GLint blendSrcRgb = 0, blendDstRgb = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
  GLboolean blend = GL_FALSE, scissor = GL_FALSE, cull = GL_FALSE, depth = GL_FALSE;

  void Save()
  {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
    blend = glIsEnabled(GL_BLEND);
    scissor = glIsEnabled(GL_SCISSOR_TEST);
    cull = glIsEnabled(GL_CULL_FACE);
    depth = glIsEnabled(GL_DEPTH_TEST);
  }

  void Restore() const
  {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(static_cast<GLuint>(program));
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
    glActiveTexture(static_cast<GLenum>(activeTexture));
    glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb), static_cast<GLenum>(blendDstRgb),
                        static_cast<GLenum>(blendSrcAlpha), static_cast<GLenum>(blendDstAlpha));
    const auto set = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
    set(GL_BLEND, blend);
    set(GL_SCISSOR_TEST, scissor);
    set(GL_CULL_FACE, cull);
    set(GL_DEPTH_TEST, depth);
  }
};

} // namespace

Renderer::~Renderer()
{
  Free(m_scene);
  Free(m_soft);
  Free(m_half);
  Free(m_quarter);
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao);
  for (GLuint program : {m_pointProgram, m_bandProgram, m_quadProgram})
    if (program)
      glDeleteProgram(program);
}

bool Renderer::Init()
{
  m_pointProgram = Link(kPointVertex, kPointFragment, m_error);
  m_bandProgram = m_pointProgram ? Link(kBandVertex, kBandFragment, m_error) : 0;
  m_quadProgram = m_bandProgram ? Link(kQuadVertex, kQuadFragment, m_error) : 0;
  if (!m_quadProgram)
    return false;
  m_pointTarget = glGetUniformLocation(m_pointProgram, "uTarget");
  m_bandTarget = glGetUniformLocation(m_bandProgram, "uTarget");
  m_quadTexture = glGetUniformLocation(m_quadProgram, "uTexture");
  m_quadGain = glGetUniformLocation(m_quadProgram, "uGain");
  GLfloat range[2] = {1.0f, 64.0f};
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  m_maxPointSize = range[1];
  GLint vao = 0, buffer = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(static_cast<GLuint>(vao));
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
  return true;
}

bool Renderer::Ensure(Target& target, int width, int height)
{
  width = std::max(1, width);
  height = std::max(1, height);
  if (target.fbo && target.width == width && target.height == height)
    return true;
  Free(target);
  glGenTextures(1, &target.texture);
  glBindTexture(GL_TEXTURE_2D, target.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    m_error = "Framebuffer unvollständig";
    Free(target);
    return false;
  }
  target.width = width;
  target.height = height;
  return true;
}

void Renderer::Free(Target& target)
{
  if (target.fbo)
    glDeleteFramebuffers(1, &target.fbo);
  if (target.texture)
    glDeleteTextures(1, &target.texture);
  target = Target{};
}

void Renderer::DrawPoints(int targetWidth, int targetHeight)
{
  if (m_points.empty())
    return;
  glUseProgram(m_pointProgram);
  glUniform2f(m_pointTarget, static_cast<float>(targetWidth), static_cast<float>(targetHeight));
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_points.size() * sizeof(float)), m_points.data(),
               GL_STREAM_DRAW);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_points.size() / 6));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
}

void Renderer::DrawBands(int targetWidth, int targetHeight)
{
  if (m_bands.empty())
    return;
  glUseProgram(m_bandProgram);
  glUniform2f(m_bandTarget, static_cast<float>(targetWidth), static_cast<float>(targetHeight));
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_bands.size() * sizeof(float)), m_bands.data(),
               GL_STREAM_DRAW);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_bands.size() / 6));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

void Renderer::DrawTexture(GLuint texture, float gain)
{
  glUseProgram(m_quadProgram);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(m_quadTexture, 0);
  glUniform1f(m_quadGain, gain);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::AppendBand(const Annulus& band, float scale)
{
  float r0 = band.r0 * scale, r1 = band.r1 * scale;
  Color inner = band.inner, outer = band.outer;
  if (band.hairline && r1 - r0 < 1.0f)
  {
    // dünner als ein Pixel: 1 Pixel breit, Deckkraft entsprechend geringer (wie Canvas)
    const float coverage = std::max(0.0f, r1 - r0), mid = (r0 + r1) / 2.0f;
    r0 = mid - 0.5f;
    r1 = mid + 0.5f;
    inner.a *= coverage;
    outer.a *= coverage;
  }
  if (inner.a <= 0.0f && outer.a <= 0.0f)
    return;
  const float cx = band.cx * scale, cy = band.cy * scale, span = band.angle1 - band.angle0;
  const int segments = std::max(4, static_cast<int>(std::ceil(std::fabs(span) * std::max(r1, 8.0f) / 6.0f)));
  const auto push = [this](float x, float y, const Color& c) {
    m_bands.insert(m_bands.end(), {x, y, c.r, c.g, c.b, c.a});
  };
  for (int s = 0; s < segments; ++s)
  {
    const float a0 = band.angle0 + span * static_cast<float>(s) / segments;
    const float a1 = band.angle0 + span * static_cast<float>(s + 1) / segments;
    const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
    push(cx + c0 * r0, cy + s0 * r0, inner);
    push(cx + c0 * r1, cy + s0 * r1, outer);
    push(cx + c1 * r1, cy + s1 * r1, outer);
    push(cx + c0 * r0, cy + s0 * r0, inner);
    push(cx + c1 * r1, cy + s1 * r1, outer);
    push(cx + c1 * r0, cy + s1 * r0, inner);
  }
}

void Renderer::Draw(const DrawList& list)
{
  SavedState kodi;
  kodi.Save();
  const int width = kodi.viewport[2], height = kodi.viewport[3];
  if (width <= 0 || height <= 0 || !m_quadProgram)
    return;
  if (!Ensure(m_scene, width, height) || !Ensure(m_soft, width / 2, height / 2) ||
      !Ensure(m_half, width / 2, height / 2) || !Ensure(m_quarter, width / 4, height / 4))
  {
    kodi.Restore();
    return;
  }
  const float scale = static_cast<float>(height) / list.height; // logisch -> Gerätepixel

  glBindVertexArray(m_vao);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  // 1. Weiche Corona in halber Auflösung
  glBindFramebuffer(GL_FRAMEBUFFER, m_soft.fbo);
  glViewport(0, 0, m_soft.width, m_soft.height);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  m_points.clear();
  const float softScale = scale * static_cast<float>(m_soft.height) / static_cast<float>(height);
  for (const auto& d : list.soft)
  {
    const float x = std::floor(d.x * softScale), y = std::floor(d.y * softScale);
    if (x < 0.0f || y < 0.0f || x >= m_soft.width || y >= m_soft.height)
      continue;
    m_points.insert(m_points.end(), {x + 0.5f, y + 0.5f, 1.0f, d.r, d.g, d.b});
  }
  glBlendFunc(GL_ONE, GL_ONE);
  DrawPoints(m_soft.width, m_soft.height);

  // 2. Szene in voller Auflösung
  glBindFramebuffer(GL_FRAMEBUFFER, m_scene.fbo);
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);
  m_bands.clear();
  for (const auto& band : list.under)
    AppendBand(band, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  DrawBands(width, height);
  glBlendFunc(GL_ONE, GL_ONE);
  DrawTexture(m_soft.texture, 1.0f);
  m_points.clear();
  const PixelGrid grid = ComputePixelGrid(list.pixel, scale);
  const float gain = list.pixelGain * grid.alpha;
  for (const auto& d : list.dust)
  {
    const float size = std::min(m_maxPointSize, static_cast<float>(grid.grid) * d.size);
    const float x = std::floor(d.x * scale / grid.grid) * grid.grid;
    const float y = std::floor(d.y * scale / grid.grid) * grid.grid;
    if (x < 0.0f || y < 0.0f || x >= width || y >= height)
      continue;
    m_points.insert(m_points.end(), {x + size / 2.0f, y + size / 2.0f, size, d.r * gain, d.g * gain, d.b * gain});
  }
  DrawPoints(width, height);
  m_bands.clear();
  for (const auto& band : list.over)
    AppendBand(band, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  DrawBands(width, height);

  // 3. Bloom: zweimal halbieren
  glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, m_half.fbo);
  glViewport(0, 0, m_half.width, m_half.height);
  DrawTexture(m_scene.texture, 1.0f);
  glBindFramebuffer(GL_FRAMEBUFFER, m_quarter.fbo);
  glViewport(0, 0, m_quarter.width, m_quarter.height);
  DrawTexture(m_half.texture, 1.0f);

  // 4. In Kodis Framebuffer zusammensetzen
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(kodi.framebuffer));
  glViewport(kodi.viewport[0], kodi.viewport[1], width, height);
  if (kodi.scissor)
    glEnable(GL_SCISSOR_TEST);
  DrawTexture(m_scene.texture, 1.0f);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  DrawTexture(m_quarter.texture, list.bloom);

  // 5. Schwarzes Loch und weißer Rand, deckend
  m_bands.clear();
  AppendBand({list.holeX, list.holeY, 0.0f, list.holeRadius, 0.0f, 6.283185307f, {0.0f, 0.0f, 0.0f, 1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}, false},
             scale);
  AppendBand(list.holeRing, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  DrawBands(width, height);

  kodi.Restore();
}

} // namespace spektra
```

- [ ] **Step 2: Kodi-Instanz auf Szene und Renderer umstellen, Diagnose entfernen**

```bash
rm src/gl/DiagnosticRenderer.h src/gl/DiagnosticRenderer.cpp
```

`src/addon.h`:

```cpp
#pragma once

#include "scene/DrawList.h"

#include <kodi/addon-instance/Visualization.h>

#include <chrono>
#include <memory>
#include <string>

namespace spektra
{
class Engine;
class Renderer;
} // namespace spektra

// Kodi-Visualisierung Spektra. Kodi behält die Instanz über Songwechsel (bei Formatwechsel Stop+Start)
// und zerstört sie, wenn das Visualisierungsfenster schließt. Analyse, Einstellungen und Szene liegen
// deshalb prozessweit; die .so ist mit -z nodelete gelinkt, damit dlclose sie nicht entlädt.
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
  std::unique_ptr<spektra::Renderer> m_renderer; // GL-Objekte gehören zur Instanz
  spektra::DrawList m_drawList;
  bool m_rendererFailed = false;
  std::chrono::steady_clock::time_point m_fpsStart = std::chrono::steady_clock::now();
  int m_frames = 0;
  float m_fps = 0.0f;
};
```

In `src/addon.cpp`:
- `#include "gl/DiagnosticRenderer.h"` ersetzen durch `#include "gl/Renderer.h"` und `#include "scene/eclipse/EclipseScene.h"`.
- Nach `SharedEngine()` einfügen:

```cpp
// Prozessweit wie die Engine: die Szene läuft beim Wiederöffnen des Fensters weiter.
spektra::EclipseScene& SharedScene()
{
  static spektra::EclipseScene scene;
  return scene;
}
```

- `Render()` ersetzen durch:

```cpp
void CSpektra::Render()
{
  if (!m_renderer && !m_rendererFailed)
  {
    auto renderer = std::make_unique<spektra::Renderer>();
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
  auto& scene = SharedScene();
  const int width = Width(), height = Height();
  if (width > 0 && height > 0)
    scene.SetViewport(720.0f * static_cast<float>(width) / static_cast<float>(height), 720.0f);
  scene.Frame(features, m_engine.CurrentSettings(), m_engine.LastDt(), m_drawList);
  m_renderer->Draw(m_drawList);

  ++m_frames;
  const auto now = std::chrono::steady_clock::now();
  const float elapsed = std::chrono::duration<float>(now - m_fpsStart).count();
  if (elapsed >= 1.0f)
  {
    m_fps = static_cast<float>(m_frames) / elapsed;
    m_frames = 0;
    m_fpsStart = now;
  }
  m_engine.Heartbeat("running", m_fps, "");
}
```

`CMakeLists.txt`: in `SPEKTRA_SOURCES` `src/gl/DiagnosticRenderer.cpp` ersetzen durch

```
                    src/gl/Renderer.cpp
                    src/scene/eclipse/Corona.cpp
                    src/scene/eclipse/DustPool.cpp
                    src/scene/eclipse/EclipseScene.cpp
                    src/scene/eclipse/Flame.cpp
```

und in `SPEKTRA_HEADERS` `src/gl/DiagnosticRenderer.h` ersetzen durch

```
                    src/gl/Renderer.h
                    src/scene/DrawList.h
                    src/scene/PixelGrid.h
                    src/scene/Random.h
                    src/scene/Scene.h
                    src/scene/eclipse/Context.h
                    src/scene/eclipse/Corona.h
                    src/scene/eclipse/DustPool.h
                    src/scene/eclipse/EclipseScene.h
                    src/scene/eclipse/Flame.h
                    src/scene/eclipse/Palette.h
```

- [ ] **Step 3: Alles prüfen**

Run: `./test.sh`
Expected: Prüfskripte 6/6 und 7/7, C++ 75/75, Build Exit 0, `check_elf` ohne Meldung.

- [ ] **Step 4: Auf dem Pi installieren**

```bash
scp -q dist/visualization.spektra-0.1.0.zip root@192.168.178.10:/storage/downloads/
ssh root@192.168.178.10 'cd /storage/.kodi/addons && rm -rf visualization.spektra && unzip -q /storage/downloads/visualization.spektra-0.1.0.zip && rm /storage/downloads/visualization.spektra-0.1.0.zip'
```

Kodi lädt die neue `.so` erst nach einem Neustart (sie ist mit `-z nodelete` gelinkt). Vor dem Neustart fragen, ob gerade jemand Kodi nutzt; dann `ssh root@192.168.178.10 'systemctl restart kodi'` und 30 s warten.

- [ ] **Step 5: Sicht- und Leistungsprüfung mit Screenshots**

Mit einer 48-kHz-Datei (z. B. `ffmpeg -i song.mp3 -ar 48000 song48.mp3`), per JSON-RPC wie in Plan 1 (`Player.Open`, `ActivateWindow(visualisation)`):

1. 20 s Musik, dann drei Screenshots (`kodi-send --action="TakeScreenshot"`), herunterladen und ansehen. Erwartet: schwarzes Loch mit weißem Rand, Flamme aus Pixelstaub rundum, goldener Saum, Wellen, je nach Melodie Streamer/Bögen – vergleichbar mit dem Prototyp (Referenzbild: `docs/entwuerfe/dune-v14-favorit.html` im Browser mit Testsong).
2. `renderer.json` lesen: `fps` ≥ 30.
3. Pause, 6 s warten, Screenshot: glimmender Ring.
4. Pixelgröße per `live.json` (`eclipse.pixel` 0 und 8, je eigene `revision`), je ein Screenshot; danach `live.json` löschen.
5. OSD-Prüfung: während der Visualisierung `kodi-send --action="ShowOSD"` und Screenshot: Kodis OSD ist korrekt über der Szene gezeichnet (Zustand wiederhergestellt).
6. Wiedergabe stoppen, Testdatei, `live.json` und alle eigenen Screenshots auf dem Pi löschen (einzeln, BusyBox kennt keine `{…}`-Muster).

Ergebnis (fps, Auffälligkeiten) ins Ledger.

- [ ] **Step 6: Stand prüfen (nicht committen)**
