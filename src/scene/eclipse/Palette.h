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
