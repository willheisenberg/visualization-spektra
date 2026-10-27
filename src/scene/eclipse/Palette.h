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

// Farbschema: Flammen- und Staubverlauf (heiß -> kalt), Corona nah -> fern, Ringtönung leise -> laut,
// Glut am Ring. Schema 0 sind die Farben des Prototyps.
struct Palette
{
  Rgb fire[5];
  Rgb coronaNear, coronaMid, coronaFar;
  Rgb tintQuiet, tintLoud, glow;
};

inline const Palette& PaletteFor(int index)
{
  static const Palette palettes[4] = {
      // Feuer: weiß -> silber -> gold -> glut -> rubin, Corona bernstein -> beige -> eisblau
      {{kWhite, kSilver, kGold, kEmber, kRuby}, kAmber, kBeige, kIce, kSilver, kGold, kGold},
      // Eis
      {{kWhite, {214.0f, 232.0f, 255.0f}, {130.0f, 186.0f, 255.0f}, {64.0f, 112.0f, 232.0f}, {44.0f, 42.0f, 164.0f}},
       {150.0f, 204.0f, 255.0f},
       {206.0f, 224.0f, 250.0f},
       {120.0f, 150.0f, 255.0f},
       kSilver,
       {150.0f, 192.0f, 255.0f},
       {120.0f, 182.0f, 255.0f}},
      // Glut
      {{{255.0f, 240.0f, 204.0f}, kAmber, {255.0f, 140.0f, 40.0f}, kEmber, kRuby},
       kAmber,
       {255.0f, 172.0f, 92.0f},
       {255.0f, 118.0f, 60.0f},
       kGold,
       kAmber,
       {255.0f, 140.0f, 40.0f}},
      // Silber
      {{kWhite, kSilver, {172.0f, 178.0f, 190.0f}, {112.0f, 118.0f, 130.0f}, {62.0f, 64.0f, 74.0f}},
       kSilver,
       {204.0f, 208.0f, 218.0f},
       {160.0f, 166.0f, 178.0f},
       kSilver,
       kWhite,
       kSilver},
  };
  return palettes[std::min(3, std::max(0, index))];
}

// Flammenfarbe nach Hitze h (0 heiß .. 1 kalt), in 64 Stufen wie die Tabelle des Prototyps.
inline Rgb FireColor(const Palette& palette, float h)
{
  const int step = static_cast<int>(std::min(0.999f, std::max(0.0f, h)) * 63.0f);
  const float x = std::min(0.999f, static_cast<float>(step) / 63.0f) * 4.0f;
  const int i = static_cast<int>(x);
  return Mix(palette.fire[i], palette.fire[i + 1], x - static_cast<float>(i));
}

inline Rgb FireColor(float h)
{
  return FireColor(PaletteFor(0), h);
}

} // namespace spektra
