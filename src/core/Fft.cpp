#include "Fft.h"

#include <cmath>
#include <stdexcept>

namespace spektra
{

Fft::Fft(size_t size) : m_n(size)
{
  if (size < 2 || (size & (size - 1)) != 0)
    throw std::invalid_argument("FFT-Größe muss eine Zweierpotenz >= 2 sein");
  m_buffer.resize(m_n);
  m_twiddle.resize(m_n / 2);
  for (size_t k = 0; k < m_n / 2; ++k)
  {
    const double angle = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(m_n);
    m_twiddle[k] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
  }
  m_reverse.resize(m_n);
  size_t bits = 0;
  while ((size_t{1} << bits) < m_n)
    ++bits;
  for (size_t i = 0; i < m_n; ++i)
  {
    size_t r = 0;
    for (size_t b = 0; b < bits; ++b)
      if (i & (size_t{1} << b))
        r |= size_t{1} << (bits - 1 - b);
    m_reverse[i] = r;
  }
}

void Fft::Magnitudes(const float* input, float* magnitudes)
{
  for (size_t i = 0; i < m_n; ++i)
    m_buffer[m_reverse[i]] = {input[i], 0.0f};
  for (size_t len = 2; len <= m_n; len <<= 1)
  {
    const size_t half = len / 2, step = m_n / len;
    for (size_t start = 0; start < m_n; start += len)
      for (size_t j = 0; j < half; ++j)
      {
        const auto t = m_twiddle[j * step] * m_buffer[start + j + half];
        const auto u = m_buffer[start + j];
        m_buffer[start + j] = u + t;
        m_buffer[start + j + half] = u - t;
      }
  }
  for (size_t k = 0; k < m_n / 2; ++k)
    magnitudes[k] = std::abs(m_buffer[k]);
}

} // namespace spektra
