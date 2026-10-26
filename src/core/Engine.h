#pragma once

#include "AudioAnalyzer.h"
#include "SampleRing.h"
#include "Settings.h"
#include "Spectrum.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace spektra
{

struct EngineConfig
{
  std::string stateDirectory;
  std::function<double()> clock;
};

// Einmal pro Prozess: Kodi zerstört die Visualisierungs-Instanz beim Schließen des Fensters,
// Analysezustand und Einstellungen sollen das überleben.
class Engine
{
public:
  explicit Engine(EngineConfig config);

  void Start(int channels, int sampleRate);
  void PushAudio(const float* interleaved, size_t count);
  const Features& Tick();

  void ApplyKodiSetting(const std::string& key, const std::string& value);
  void ReplaceSettings(const Settings& settings);
  const Settings& CurrentSettings() const { return m_settings; }

  float LastDt() const { return m_dt; }
  void Heartbeat(const std::string& state, float fps, const std::string& error);

private:
  void CheckLiveSettings(double now);

  EngineConfig m_config;
  SampleRing m_ring;
  Spectrum m_spectrum;
  AudioAnalyzer m_analyzer;
  Settings m_settings = Settings::Defaults();
  std::vector<float> m_samples, m_silence;
  float m_sampleRate = 44100.0f;

  std::mutex m_pushMutex;
  double m_lastPush = -1e9;

  double m_lastTick = -1.0;
  double m_lastLiveCheck = -1e9;
  double m_lastHeartbeat = -1e9;
  int64_t m_liveRevision = 0;
  bool m_inGap = false;
  float m_dt = 0.0f;
};

} // namespace spektra
