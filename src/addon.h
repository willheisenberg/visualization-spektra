#pragma once

#include "scene/DrawList.h"

#include <kodi/addon-instance/Visualization.h>

#include <chrono>
#include <memory>
#include <string>

namespace spektra
{
class Engine;
class Renderer;
} // namespace spektra

// Kodi-Visualisierung Spektra. Kodi behält die Instanz über Songwechsel (bei Formatwechsel Stop+Start)
// und zerstört sie, wenn das Visualisierungsfenster schließt. Analyse, Einstellungen und Szene liegen
// deshalb prozessweit; die .so ist mit -z nodelete gelinkt, damit dlclose sie nicht entlädt.
class ATTR_DLL_LOCAL CSpektra : public kodi::addon::CAddonBase,
                                public kodi::addon::CInstanceVisualization
{
public:
  CSpektra();
  ~CSpektra() override;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::addon::CSettingValue& settingValue) override;
  bool Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName) override;
  void AudioData(const float* audioData, size_t audioDataLength) override;
  bool IsDirty() override { return true; }
  void Render() override;

private:
  spektra::Engine& m_engine;
  std::unique_ptr<spektra::Renderer> m_renderer; // GL-Objekte gehören zur Instanz
  spektra::DrawList m_drawList;
  bool m_rendererFailed = false;
  std::chrono::steady_clock::time_point m_fpsStart = std::chrono::steady_clock::now();
  int m_frames = 0;
  float m_fps = 0.0f;
};
