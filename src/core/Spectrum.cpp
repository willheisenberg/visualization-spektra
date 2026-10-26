#include "Spectrum.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

Spectrum::Spectrum(SpectrumConfig config) : m_config(config), m_fft(config.fftSize)
{
  const size_t n = m_config.fftSize;
  m_window.resize(n);
  for (size_t i = 0; i < n; ++i)
  {
    const double x = static_cast<double>(i) / static_cast<double>(n);
    m_window[i] = static_cast<float>(0.42 - 0.5 * std::cos(2.0 * M_PI * x) + 0.08 * std::cos(4.0 * M_PI * x));
  }
  m_windowed.resize(n);
  m_magnitudes.resize(n / 2);
  m_smoothed.assign(n / 2, 0.0f);
  m_linear.assign(n / 2, 0.0f);
  m_normalized.assign(n / 2, 0.0f);
  const float range = m_config.maxDb - m_config.minDb;
  for (int b = 0; b < 256; ++b)
    m_byteToLinear[b] = b == 0 ? 0.0f
                               : std::pow(10.0f, (m_config.minDb + range * b / 255.0f - m_config.maxDb) / 20.0f);
}

void Spectrum::Reset()
{
  std::fill(m_smoothed.begin(), m_smoothed.end(), 0.0f);
}

void Spectrum::Process(const float* samples)
{
  const size_t n = m_config.fftSize;
  for (size_t i = 0; i < n; ++i)
    m_windowed[i] = samples[i] * m_window[i];
  m_fft.Magnitudes(m_windowed.data(), m_magnitudes.data());
  const float tau = m_config.smoothing, range = m_config.maxDb - m_config.minDb;
  for (size_t k = 0; k < n / 2; ++k)
  {
    m_smoothed[k] = tau * m_smoothed[k] + (1.0f - tau) * m_magnitudes[k] / static_cast<float>(n);
    int byte = 0;
    if (m_smoothed[k] > 0.0f)
    {
      const float db = 20.0f * std::log10(m_smoothed[k]);
      byte = static_cast<int>(std::floor(255.0f / range * (db - m_config.minDb)));
      byte = std::clamp(byte, 0, 255);
    }
    m_normalized[k] = byte / 255.0f;
    m_linear[k] = m_byteToLinear[byte];
  }
}

} // namespace spektra
