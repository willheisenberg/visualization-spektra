#include "core/AudioAnalyzer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace spektra;

namespace
{
constexpr float kBinHz = 44100.0f / 4096.0f;
constexpr float kDt = 1.0f / 60.0f;

// Spektrum mit konstantem Wert in [lo, hi) Hz, sonst 0.
std::vector<float> Bins(std::initializer_list<std::tuple<float, float, float>> ranges)
{
  std::vector<float> v(2048, 0.0f);
  for (const auto& [lo, hi, value] : ranges)
    for (size_t k = static_cast<size_t>(lo / kBinHz); k < v.size() && k * kBinHz < hi; ++k)
      v[k] = value;
  return v;
}

struct Runner
{
  AudioAnalyzer analyzer;
  double t = 0.0;
  const Features& Step(const std::vector<float>& linear)
  {
    t += kDt;
    std::vector<float> norm(linear.size());
    for (size_t i = 0; i < linear.size(); ++i)
      norm[i] = linear[i] > 0.0f ? 0.5f : 0.0f;
    return analyzer.Update(linear, norm, kBinHz, t, kDt);
  }
  void Run(const std::vector<float>& linear, double seconds)
  {
    for (int i = 0; i < static_cast<int>(seconds / kDt); ++i)
      Step(linear);
  }
};
} // namespace

TEST(AudioAnalyzerTest, SilenceGivesZeroLevels)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({}), 2.0);
  for (float l : r.analyzer.Current().level)
    EXPECT_EQ(l, 0.0f);
  EXPECT_FALSE(r.analyzer.Current().drop);
}

TEST(AudioAnalyzerTest, SteadyMidSettlesAtRelativeLevel)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  // Abstand zu den Bandgrenzen: das Grenz-Bin bei 3 kHz gehört zu Mitten und Höhen.
  r.Run(Bins({{600.0f, 2800.0f, 0.2f}}), 10.0);
  const auto& f = r.analyzer.Current();
  EXPECT_NEAR(f.level[kMid], std::pow(1.0f / 1.7f, 1.3f), 0.02f);
  EXPECT_EQ(f.level[kBass], 0.0f);
  EXPECT_EQ(f.level[kHigh], 0.0f);
}

TEST(AudioAnalyzerTest, IntensityScalesLevels)
{
  Runner r;
  r.analyzer.SetIntensity(0.25f);
  r.Run(Bins({{500.0f, 3000.0f, 0.2f}}), 10.0);
  EXPECT_NEAR(r.analyzer.Current().level[kMid], std::pow(1.0f / 1.7f, 1.3f) * std::pow(0.25f, 0.6f), 0.02f);
}

TEST(AudioAnalyzerTest, KicksBecomeBassOnsets)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  // Kick nur bis 100 Hz, damit kein Bin in den Bereich der Tiefen (ab Bin 11) fällt.
  const auto quiet = Bins({{25.0f, 100.0f, 0.02f}});
  const auto kick = Bins({{25.0f, 100.0f, 0.5f}});
  r.Run(quiet, 2.0);
  int bassOnsets = 0, otherOnsets = 0;
  for (int beat = 0; beat < 8; ++beat)
  {
    for (int i = 0; i < 30; ++i) // 0,5 s pro Schlag, Kick die ersten 3 Bilder
    {
      const auto& f = r.Step(i < 3 ? kick : quiet);
      bassOnsets += f.onset[kBass] > 0.0f;
      otherOnsets += f.onset[kLow] > 0.0f || f.onset[kMid] > 0.0f || f.onset[kHigh] > 0.0f;
    }
  }
  EXPECT_EQ(bassOnsets, 8);
  EXPECT_EQ(otherOnsets, 0);
}

TEST(AudioAnalyzerTest, CentroidFollowsPitch)
{
  Runner low, high;
  low.analyzer.SetIntensity(1.0f);
  high.analyzer.SetIntensity(1.0f);
  std::vector<float> lowNorm(2048, 0.0f), highNorm(2048, 0.0f);
  for (int k = 46; k <= 50; ++k) lowNorm[k] = 0.8f;    // ~500 Hz
  for (int k = 270; k <= 278; ++k) highNorm[k] = 0.8f; // ~3 kHz
  std::vector<float> lin(2048, 0.0f);
  for (int i = 0; i < 120; ++i)
  {
    low.t += kDt; high.t += kDt;
    low.analyzer.Update(lin, lowNorm, kBinHz, low.t, kDt);
    high.analyzer.Update(lin, highNorm, kBinHz, high.t, kDt);
  }
  EXPECT_LT(low.analyzer.Current().centroid, 0.1f);
  EXPECT_GT(high.analyzer.Current().centroid, 0.9f);
}

TEST(AudioAnalyzerTest, KickAfterBasslessPassageIsDrop)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{500.0f, 3000.0f, 0.3f}}), 6.0); // Breakdown: nur Mitten
  const auto& f = r.Step(Bins({{25.0f, 120.0f, 0.6f}, {500.0f, 3000.0f, 0.3f}}));
  EXPECT_GT(f.onset[kBass], 0.35f);
  EXPECT_TRUE(f.drop);
}

TEST(AudioAnalyzerTest, NoDropWhileBassIsRunning)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  const auto quiet = Bins({{25.0f, 120.0f, 0.1f}, {500.0f, 3000.0f, 0.3f}});
  const auto kick = Bins({{25.0f, 120.0f, 0.6f}, {500.0f, 3000.0f, 0.3f}});
  bool anyDrop = false;
  for (int beat = 0; beat < 20; ++beat)
    for (int i = 0; i < 30; ++i)
      anyDrop |= r.Step(i < 3 ? kick : quiet).drop && beat > 4;
  EXPECT_FALSE(anyDrop);
}

TEST(AudioAnalyzerTest, RebaseResetsLongTermAverage)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{500.0f, 3000.0f, 0.8f}}), 20.0);
  r.analyzer.Rebase();
  r.Run(Bins({{500.0f, 3000.0f, 0.1f}}), 1.0);
  EXPECT_NEAR(r.analyzer.Current().level[kMid], std::pow(1.0f / 1.7f, 1.3f), 0.05f);
}

TEST(AudioAnalyzerTest, FineSpectrumFollowsBands)
{
  Runner r;
  r.analyzer.SetIntensity(1.0f);
  r.Run(Bins({{25.0f, 120.0f, 0.3f}}), 3.0);
  const auto& fine = r.analyzer.Current().fine;
  EXPECT_GT(fine[2], 0.2f);
  EXPECT_EQ(fine[30], 0.0f);
}
