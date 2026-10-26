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
