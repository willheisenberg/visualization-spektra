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
