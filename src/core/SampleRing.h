#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace spektra
{

// Mischt interleavte Kanäle zu Mono und hält die jüngsten Samples. Kodi ruft Push aus seinem
// Audio-Thread, Latest wird aus Render() gelesen.
class SampleRing
{
public:
  explicit SampleRing(size_t capacity = 16384);

  void SetChannels(int channels);
  void Push(const float* interleaved, size_t count);
  void Clear(); // alte Samples verwerfen (nach einer Lücke im Audio)
  bool Latest(float* destination, size_t frames) const;
  uint64_t TotalFrames() const;

private:
  mutable std::mutex m_mutex;
  std::vector<float> m_buffer;
  size_t m_write = 0;
  uint64_t m_total = 0;
  int m_channels = 2;
  float m_partialSum = 0.0f;
  int m_partialCount = 0;
};

} // namespace spektra
