#pragma once

#include <array>
#include <vector>

namespace spektra
{

enum Band : int
{
  kBass = 0,
  kLow = 1,
  kMid = 2,
  kHigh = 3
};
constexpr int kBandCount = 4;
constexpr int kFineBands = 32;

struct Features
{
  std::array<float, kBandCount> level{};
  std::array<float, kBandCount> onset{};
  float centroid = 0.5f;
  float loudness = 0.0f;
  bool drop = false;
  std::array<float, kFineBands> fine{};
};

// Pegel, Anschläge, Tonhöhe, Intensität und Drop wie im Prototyp (Spec §5).
class AudioAnalyzer
{
public:
  void SetIntensity(float m);
  float Intensity() const { return m_intensity; }

  const Features& Update(const std::vector<float>& linear, const std::vector<float>& normalized, float binHz,
                         double t, float dt);
  void Rebase();
  const Features& Current() const { return m_features; }

private:
  struct BandState
  {
    float longAvg = 0.0f;
    float avg = 0.0f;
    float prev = 0.0f;
    double last = -9.0;
  };

  float m_intensity = 0.7f;
  Features m_features;
  std::array<BandState, kBandCount> m_bands{};
  std::array<float, kFineBands> m_finePeak{};
  float m_loudAvg = 0.0f;
  float m_loudMax = 1e-3f;
  float m_bassHistory = 0.0f;
  double m_lastDrop = -1e9;
};

} // namespace spektra
