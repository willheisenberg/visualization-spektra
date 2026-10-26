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
