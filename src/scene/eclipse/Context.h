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
  float flowT = 0.0f, flowStrength = 0.0f;
  float flowTempo = 1.0f; // wie schnell freier Staub der Strömung folgt (Regler Strömung: Tempo)
  float reach = 1.0f; // wie weit freier Staub treibt und wie lange er lebt
  int palette = 0;    // Farbschema, siehe PaletteFor()
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
