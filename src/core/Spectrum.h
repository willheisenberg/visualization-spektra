#pragma once

#include "Fft.h"

#include <cstddef>
#include <vector>

namespace spektra
{

struct SpectrumConfig
{
  size_t fftSize = 4096;
  float smoothing = 0.5f;
  float minDb = -100.0f;
  float maxDb = -30.0f;
};

// Nachbau von WebAudio AnalyserNode.getByteFrequencyData plus lineare Rückrechnung,
// damit die Analyse dieselben Zahlen sieht wie der Browser-Prototyp.
class Spectrum
{
public:
  explicit Spectrum(SpectrumConfig config = {});

  size_t FftSize() const { return m_config.fftSize; }
  size_t Bins() const { return m_config.fftSize / 2; }

  void Process(const float* samples);
  void Reset();

  const std::vector<float>& Linear() const { return m_linear; }
  const std::vector<float>& Normalized() const { return m_normalized; }

private:
  SpectrumConfig m_config;
  Fft m_fft;
  std::vector<float> m_window, m_windowed, m_magnitudes, m_smoothed, m_linear, m_normalized;
  float m_byteToLinear[256];
};

} // namespace spektra
