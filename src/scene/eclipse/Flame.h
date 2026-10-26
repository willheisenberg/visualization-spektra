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
