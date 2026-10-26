#include "core/Spectrum.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using spektra::Spectrum;

namespace
{
std::vector<float> Sine(double hz, float amplitude, size_t n = 4096, double rate = 44100.0)
{
  std::vector<float> s(n);
  for (size_t i = 0; i < n; ++i)
    s[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * hz * i / rate));
  return s;
}
} // namespace

TEST(SpectrumTest, SilenceIsZero)
{
  Spectrum spectrum;
  std::vector<float> silence(4096, 0.0f);
  spectrum.Process(silence.data());
  for (float v : spectrum.Linear())
    EXPECT_EQ(v, 0.0f);
}

TEST(SpectrumTest, LoudSineSaturatesItsBinOnly)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 1.0f);
  spectrum.Process(s.data());
  const size_t bin = static_cast<size_t>(std::round(1000.0 / (44100.0 / 4096.0)));
  EXPECT_FLOAT_EQ(spectrum.Linear()[bin], 1.0f);
  EXPECT_FLOAT_EQ(spectrum.Normalized()[bin], 1.0f);
  EXPECT_EQ(spectrum.Linear()[500], 0.0f);
}

TEST(SpectrumTest, QuietSineMapsThroughByteScale)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 0.001f);
  spectrum.Process(s.data());
  const size_t bin = 93;
  const float lin = spectrum.Linear()[bin];
  EXPECT_GT(lin, 0.002f);
  EXPECT_LT(lin, 0.02f);
  // Rückrechnung: Byte b -> 10^((-100 + 70 b/255 + 30)/20)
  const float b = spectrum.Normalized()[bin] * 255.0f;
  EXPECT_NEAR(lin, std::pow(10.0f, (-100.0f + 70.0f * b / 255.0f + 30.0f) / 20.0f), 1e-6f);
}

TEST(SpectrumTest, SmoothingDecaysAfterSilence)
{
  Spectrum spectrum;
  const auto s = Sine(1000.0, 0.001f);
  std::vector<float> silence(4096, 0.0f);
  spectrum.Process(s.data());
  const float first = spectrum.Linear()[93];
  spectrum.Process(silence.data());
  const float second = spectrum.Linear()[93];
  EXPECT_LT(second, first);
  EXPECT_GT(second, 0.0f);
  spectrum.Reset();
  spectrum.Process(silence.data());
  EXPECT_EQ(spectrum.Linear()[93], 0.0f);
}
