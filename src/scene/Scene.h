#pragma once

#include "DrawList.h"
#include "core/AudioAnalyzer.h"
#include "core/Settings.h"

namespace spektra
{

// Eine Szene simuliert ein Bild und beschreibt es als DrawList. Kein Kodi, kein GL.
class Scene
{
public:
  virtual ~Scene() = default;
  // Logische Größe: Höhe 720, Breite nach Seitenverhältnis.
  virtual void SetViewport(float width, float height) = 0;
  virtual void Frame(const Features& features, const Settings& settings, float dt, DrawList& out) = 0;
};

} // namespace spektra
