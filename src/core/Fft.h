#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace spektra
{

// Radix-2-FFT fester Größe für reelle Eingaben. Nicht thread-sicher (interner Puffer).
class Fft
{
public:
  explicit Fft(size_t size);

  size_t Size() const { return m_n; }

  // input: Size() Werte; magnitudes: Size()/2 Werte, |X[k]| ohne Normierung.
  void Magnitudes(const float* input, float* magnitudes);

private:
  size_t m_n;
  std::vector<std::complex<float>> m_buffer;
  std::vector<std::complex<float>> m_twiddle;
  std::vector<size_t> m_reverse;
};

} // namespace spektra
