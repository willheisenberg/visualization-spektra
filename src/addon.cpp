#include "addon.h"

#include "core/Engine.h"
#include "core/Settings.h"
#include "gl/Renderer.h"
#include "scene/eclipse/EclipseScene.h"

namespace
{

constexpr const char* kPrefix = "[visualization.spektra] ";

std::string StateDirectory()
{
  std::string path = kodi::addon::GetUserPath();
  while (path.size() > 1 && path.back() == '/')
    path.pop_back();
  return path;
}

// Prozessweit: überlebt das Schließen und Wiederöffnen des Fensters (.so mit -z nodelete).
spektra::Engine& SharedEngine()
{
  static spektra::Engine engine(spektra::EngineConfig{StateDirectory(), {}});
  return engine;
}

// Prozessweit wie die Engine: die Szene läuft beim Wiederöffnen des Fensters weiter.
spektra::EclipseScene& SharedScene()
{
  static spektra::EclipseScene scene;
  return scene;
}

// Alle Einstellungen typgerecht aus Kodi lesen.
spektra::Settings ReadKodiSettings()
{
  auto settings = spektra::Settings::Defaults();
  for (const auto& key : spektra::SettingKeys())
  {
    const auto type = spektra::TypeOf(key);
    if (!type)
      continue;
    std::string value;
    switch (*type)
    {
      case spektra::SettingType::Bool:
        value = kodi::addon::GetSettingBoolean(key, spektra::SettingValue(settings, key) == "true") ? "true"
                                                                                                    : "false";
        break;
      case spektra::SettingType::Int:
        value = std::to_string(kodi::addon::GetSettingInt(key, std::stoi(spektra::SettingValue(settings, key))));
        break;
      case spektra::SettingType::String:
        value = kodi::addon::GetSettingString(key, spektra::SettingValue(settings, key));
        break;
    }
    spektra::ApplySetting(settings, key, value);
  }
  return settings;
}

} // namespace

CSpektra::CSpektra() : m_engine(SharedEngine())
{
  m_engine.ReplaceSettings(ReadKodiSettings());
}

CSpektra::~CSpektra() = default;

ADDON_STATUS CSpektra::SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue)
{
  m_engine.ApplyKodiSetting(settingName, settingValue.GetString());
  return ADDON_STATUS_OK;
}

bool CSpektra::Start(int channels, int samplesPerSec, int, const std::string&)
{
  m_engine.Start(channels, samplesPerSec);
  return true;
}

void CSpektra::AudioData(const float* audioData, size_t audioDataLength)
{
  if (audioData && audioDataLength > 0)
    m_engine.PushAudio(audioData, audioDataLength);
}

void CSpektra::Render()
{
  if (!m_renderer && !m_rendererFailed)
  {
    auto renderer = std::make_unique<spektra::Renderer>();
    if (!renderer->Init())
    {
      m_rendererFailed = true;
      kodi::Log(ADDON_LOG_ERROR, "%sGL-Initialisierung fehlgeschlagen: %s", kPrefix, renderer->LastError().c_str());
    }
    m_renderer = std::move(renderer);
  }

  const auto& features = m_engine.Tick();
  if (m_rendererFailed)
  {
    m_engine.Heartbeat("error", 0.0f, "gl_init");
    return;
  }
  auto& scene = SharedScene();
  const int width = Width(), height = Height();
  if (width > 0 && height > 0)
    scene.SetViewport(720.0f * static_cast<float>(width) / static_cast<float>(height), 720.0f);
  scene.Frame(features, m_engine.CurrentSettings(), m_engine.LastDt(), m_drawList);
  m_renderer->Draw(m_drawList);

  ++m_frames;
  const auto now = std::chrono::steady_clock::now();
  const float elapsed = std::chrono::duration<float>(now - m_fpsStart).count();
  if (elapsed >= 1.0f)
  {
    m_fps = static_cast<float>(m_frames) / elapsed;
    m_frames = 0;
    m_fpsStart = now;
  }
  m_engine.Heartbeat("running", m_fps, "");
}

ADDONCREATOR(CSpektra)
