#include "core/Fft.h"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

using spektra::Fft;

TEST(FftTest, ImpulseHasFlatSpectrum)
{
  Fft fft(8);
  std::vector<float> in(8, 0.0f), mag(4);
  in[0] = 1.0f;
  fft.Magnitudes(in.data(), mag.data());
  for (float m : mag)
    EXPECT_NEAR(m, 1.0f, 1e-5f);
}

TEST(FftTest, CosineLandsInItsBin)
{
  const size_t n = 64;
  Fft fft(n);
  std::vector<float> in(n), mag(n / 2);
  for (size_t i = 0; i < n; ++i)
    in[i] = std::cos(2.0 * M_PI * 3.0 * i / n);
  fft.Magnitudes(in.data(), mag.data());
  EXPECT_NEAR(mag[3], 32.0f, 1e-3f);
  for (size_t k = 0; k < n / 2; ++k)
  {
    if (k != 3)
    {
      EXPECT_NEAR(mag[k], 0.0f, 1e-3f) << "Bin " << k;
    }
  }
}

TEST(FftTest, LargeSizeMatchesDirectDft)
{
  const size_t n = 4096;
  Fft fft(n);
  std::vector<float> in(n), mag(n / 2);
  for (size_t i = 0; i < n; ++i)
    in[i] = 0.3f * std::sin(2.0 * M_PI * 100.5 * i / n) + 0.1f * std::cos(2.0 * M_PI * 777.0 * i / n);
  fft.Magnitudes(in.data(), mag.data());
  for (size_t k : {0u, 100u, 101u, 777u, 2000u})
  {
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
      re += in[i] * std::cos(2.0 * M_PI * k * i / n);
      im -= in[i] * std::sin(2.0 * M_PI * k * i / n);
    }
    EXPECT_NEAR(mag[k], std::hypot(re, im), 0.05) << "Bin " << k;
  }
}

TEST(FftTest, RejectsNonPowerOfTwo)
{
  EXPECT_THROW(Fft(12), std::invalid_argument);
  EXPECT_THROW(Fft(1), std::invalid_argument);
}
