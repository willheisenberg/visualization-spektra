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

void AudioAnalyzer::SetTuning(const std::array<float, kBandCount>& sensitivity, float response, float onset)
{
  for (int i = 0; i < kBandCount; ++i)
    m_sensitivity[i] = std::max(0.0f, sensitivity[i]);
  m_response = std::max(0.05f, response);
  m_onset = std::max(0.05f, onset);
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
    const float sens = m_sensitivity[i];
    const float target = std::min(1.0f, std::pow(norm, 1.3f) * gain * sens);
    float& level = m_features.level[i];
    level += (target - level) * (target > level ? 0.6f : Approach(dt, spec.rel * m_response));

    m_features.onset[i] = 0.0f;
    const float ratio = 1.0f + (spec.ratio - 1.0f) * m_onset;
    if (sens > 0.0f && e > s.avg * ratio && e > s.longAvg * 0.5f && e > s.prev && t - s.last > spec.cool)
    {
      s.last = t;
      m_features.onset[i] = Clamp01((e / std::max(s.avg, 1e-4f) - 1.0f) * 0.9f * sens) * m;
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

  // Drop: kräftiger Kick nach einer Phase ohne Bass, während die Musik laut ist. Höchstens einer pro 8 s:
  // beginnt ein Song direkt mit Kicks, ist das Bassgedächtnis noch leer und jeder Kick sähe wie ein Drop aus.
  m_features.drop = m_features.onset[kBass] > 0.35f * m && m_bassHistory < 0.15f && m_loudAvg > 0.5f * m_loudMax &&
                    t - m_lastDrop > 8.0;
  if (m_features.drop)
    m_lastDrop = t;
  m_bassHistory += (m_features.level[kBass] - m_bassHistory) * Approach(dt, 3.0f);

  return m_features;
}

} // namespace spektra
