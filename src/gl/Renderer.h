#pragma once

#include "scene/DrawList.h"

#include <GLES3/gl3.h>

#include <string>
#include <vector>

namespace spektra
{

// Zeichnet eine DrawList mit GLES 3 in den aktuell gebundenen Framebuffer und Viewport (Kodi).
// GL-Objekte gehören zur Kodi-Instanz.
class Renderer
{
public:
  ~Renderer();

  bool Init();
  void Draw(const DrawList& list);
  const std::string& LastError() const { return m_error; }

private:
  struct Target
  {
    GLuint fbo = 0, texture = 0;
    int width = 0, height = 0;
  };

  bool Ensure(Target& target, int width, int height);
  void Free(Target& target);
  void DrawPoints(int targetWidth, int targetHeight);
  void DrawBands(int targetWidth, int targetHeight);
  void DrawTexture(GLuint texture, float gain);
  void AppendBand(const Annulus& band, float scale);

  GLuint m_pointProgram = 0, m_bandProgram = 0, m_quadProgram = 0;
  GLint m_pointTarget = -1, m_bandTarget = -1, m_quadTexture = -1, m_quadGain = -1;
  GLuint m_vao = 0, m_vbo = 0;
  float m_maxPointSize = 64.0f;
  Target m_scene, m_soft, m_half, m_quarter;
  std::vector<float> m_points, m_bands;
  std::string m_error;
};

} // namespace spektra
