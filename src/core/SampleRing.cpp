#include "SampleRing.h"

#include <algorithm>

namespace spektra
{

SampleRing::SampleRing(size_t capacity) : m_buffer(std::max<size_t>(capacity, 1), 0.0f)
{
}

void SampleRing::SetChannels(int channels)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_channels = std::max(1, channels);
  m_partialSum = 0.0f;
  m_partialCount = 0;
}

void SampleRing::Push(const float* interleaved, size_t count)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  for (size_t i = 0; i < count; ++i)
  {
    m_partialSum += interleaved[i];
    if (++m_partialCount < m_channels)
      continue;
    m_buffer[m_write] = m_partialSum / static_cast<float>(m_channels);
    m_write = (m_write + 1) % m_buffer.size();
    ++m_total;
    m_partialSum = 0.0f;
    m_partialCount = 0;
  }
}

void SampleRing::Clear()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_write = 0;
  m_total = 0;
  m_partialSum = 0.0f;
  m_partialCount = 0;
}

bool SampleRing::Latest(float* destination, size_t frames) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (frames > m_buffer.size() || frames > m_total)
    return false;
  size_t read = (m_write + m_buffer.size() - frames) % m_buffer.size();
  for (size_t i = 0; i < frames; ++i)
  {
    destination[i] = m_buffer[read];
    read = (read + 1) % m_buffer.size();
  }
  return true;
}

uint64_t SampleRing::TotalFrames() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_total;
}

} // namespace spektra
