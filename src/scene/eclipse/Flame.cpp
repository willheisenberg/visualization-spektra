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
      const float life = free ? (2.5f + m_rng.Uniform() * 3.0f) * c.reach : 0.5f + m_rng.Uniform() * 0.9f * std::min(1.0f, power);
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
