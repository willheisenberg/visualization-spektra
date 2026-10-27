#include "Engine.h"

#include "StateFiles.h"

#include <algorithm>
#include <chrono>

namespace spektra
{
namespace
{
constexpr size_t kFftSize = 4096;
constexpr double kSilenceAfter = 0.1; // ohne Audio: Stille analysieren
constexpr double kGapRebase = 1.0;    // so lange Lücke: neu einpegeln
} // namespace

Engine::Engine(EngineConfig config) : m_config(std::move(config))
{
  if (!m_config.clock)
    m_config.clock = [] {
      return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
  m_samples.resize(kFftSize);
  m_silence.assign(kFftSize, 0.0f);
}

void Engine::Start(int channels, int sampleRate)
{
  m_ring.SetChannels(channels);
  if (sampleRate > 0)
    m_sampleRate = static_cast<float>(sampleRate);
}

void Engine::PushAudio(const float* interleaved, size_t count)
{
  m_ring.Push(interleaved, count);
  std::lock_guard<std::mutex> lock(m_pushMutex);
  m_lastPush = m_config.clock();
}

const Features& Engine::Tick()
{
  const double now = m_config.clock();
  const float dtReal = m_lastTick < 0.0 ? 1.0f / 60.0f : static_cast<float>(std::min(1.0, now - m_lastTick));
  m_lastTick = now;
  m_dt = std::min(0.1f, dtReal);

  CheckLiveSettings(now);
  m_analyzer.SetIntensity(m_settings.intensity / 100.0f);
  m_analyzer.SetTuning({m_settings.sensBass / 100.0f, m_settings.sensLow / 100.0f, m_settings.sensMid / 100.0f,
                        m_settings.sensHigh / 100.0f},
                       m_settings.response / 100.0f, m_settings.onset / 100.0f);

  double lastPush;
  {
    std::lock_guard<std::mutex> lock(m_pushMutex);
    lastPush = m_lastPush;
  }
  const double silentFor = now - lastPush;
  if (silentFor > kGapRebase && !m_inGap)
  {
    // Lücke (Spulen, Titelwechsel): alte Samples würden das Neueinpegeln auf die Lautstärke
    // von vor der Lücke festlegen.
    m_inGap = true;
    m_ring.Clear();
  }

  const float* input = m_silence.data();
  if (silentFor <= kSilenceAfter && m_ring.Latest(m_samples.data(), kFftSize))
  {
    if (m_inGap)
    {
      m_analyzer.Rebase();
      m_spectrum.Reset();
      m_inGap = false;
    }
    input = m_samples.data();
  }
  m_spectrum.Process(input);
  return m_analyzer.Update(m_spectrum.Linear(), m_spectrum.Normalized(), m_sampleRate / kFftSize, now, dtReal);
}

void Engine::ApplyKodiSetting(const std::string& key, const std::string& value)
{
  ApplySetting(m_settings, key, value);
}

void Engine::ReplaceSettings(const Settings& settings)
{
  m_settings = settings;
}

void Engine::CheckLiveSettings(double now)
{
  if (now - m_lastLiveCheck < 0.2) // beim Kalibrieren soll jede Änderung sofort sichtbar sein
    return;
  m_lastLiveCheck = now;
  const auto live = ReadLiveSettings(m_config.stateDirectory + "/live.json", m_settings);
  if (live && live->revision > m_liveRevision)
  {
    m_liveRevision = live->revision;
    m_settings = live->settings;
  }
}

void Engine::Heartbeat(const std::string& state, float fps, const std::string& error)
{
  const double now = m_config.clock();
  if (now - m_lastHeartbeat < 1.0)
    return;
  m_lastHeartbeat = now;
  RendererStatus status;
  status.state = state;
  status.scene = m_settings.scene;
  status.fps = fps;
  status.error = error;
  WriteRendererStatus(m_config.stateDirectory + "/renderer.json", status);
}

} // namespace spektra
