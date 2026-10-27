#pragma once

#include "Corona.h"
#include "DustPool.h"
#include "Flame.h"
#include "scene/Random.h"
#include "scene/Scene.h"

#include <cstdint>
#include <vector>

namespace spektra
{

// Digitale Sonnenfinsternis (Prototyp dune-v14): schwarzes Loch, Flamme aus Pixelstaub,
// freier Staub, warme Corona, Wellen. Jedes Element hängt über die Zuordnung an einem Band.
class EclipseScene : public Scene
{
public:
  // startTime: Szenenzeit zu Beginn (Sekunden); sonst 0.
  explicit EclipseScene(uint64_t seed = 0x5EC7A11ull, double startTime = 0.0);

  void SetViewport(float width, float height) override;
  void Frame(const Features& features, const Settings& settings, float dt, DrawList& out) override;

  size_t DustCount() const { return m_dust.Size(); }
  size_t CoronaCount() const { return m_corona.Size(); }
  size_t WaveCount() const { return m_waves.size(); }

private:
  struct Wave
  {
    double t0;
    float s;
  };

  // Tonhöhe -> Stelle am Ring: tief unten, hoch oben, zufällig links oder rechts.
  float MelodyAngle(float centroid);

  Random m_rng;
  DustPool m_dust;
  Flame m_flame;
  Corona m_corona;
  std::vector<Wave> m_waves;
  float m_width = 1280.0f, m_height = 720.0f;
  // Zeiten in double: die Szene lebt so lange wie Kodi, float wäre nach Tagen zu ungenau.
  double m_t = 0.0, m_beatT = -9.0, m_flowT = 0.0;
};

} // namespace spektra
