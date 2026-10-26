#include "Renderer.h"

#include "scene/PixelGrid.h"

#include <algorithm>
#include <cmath>

namespace spektra
{
namespace
{

constexpr const char* kPointVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec3 aColor;
uniform vec2 uTarget;
out vec3 vColor;
void main() {
  gl_Position = vec4(aPos.x / uTarget.x * 2.0 - 1.0, 1.0 - aPos.y / uTarget.y * 2.0, 0.0, 1.0);
  gl_PointSize = aSize;
  vColor = aColor;
}
)";

constexpr const char* kPointFragment = R"(#version 300 es
precision highp float;
in vec3 vColor;
out vec4 outColor;
void main() { outColor = vec4(vColor, 0.0); }
)";

constexpr const char* kBandVertex = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uTarget;
out vec4 vColor;
void main() {
  gl_Position = vec4(aPos.x / uTarget.x * 2.0 - 1.0, 1.0 - aPos.y / uTarget.y * 2.0, 0.0, 1.0);
  vColor = aColor;
}
)";

constexpr const char* kBandFragment = R"(#version 300 es
precision highp float;
in vec4 vColor;
out vec4 outColor;
void main() { outColor = vColor; }
)";

constexpr const char* kQuadVertex = R"(#version 300 es
out vec2 vUv;
void main() {
  vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
  vUv = pos[gl_VertexID] * 0.5 + 0.5;
}
)";

constexpr const char* kQuadFragment = R"(#version 300 es
precision highp float;
uniform sampler2D uTexture;
uniform float uGain;
in vec2 vUv;
out vec4 outColor;
void main() { outColor = vec4(texture(uTexture, vUv).rgb * uGain, 1.0); }
)";

GLuint Compile(GLenum type, const char* source, std::string& error)
{
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok)
  {
    char log[1024] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    error = log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint Link(const char* vertex, const char* fragment, std::string& error)
{
  const GLuint vs = Compile(GL_VERTEX_SHADER, vertex, error);
  if (!vs)
    return 0;
  const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment, error);
  if (!fs)
  {
    glDeleteShader(vs);
    return 0;
  }
  const GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
  {
    char log[1024] = {};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    error = log;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

// Kodis GL-Zustand, der beim Zeichnen verändert und danach wiederhergestellt wird.
struct SavedState
{
  GLint framebuffer = 0, viewport[4] = {}, program = 0, vao = 0, arrayBuffer = 0, activeTexture = 0, texture = 0;
  GLint blendSrcRgb = 0, blendDstRgb = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
  GLboolean blend = GL_FALSE, scissor = GL_FALSE, cull = GL_FALSE, depth = GL_FALSE;

  void Save()
  {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
    blend = glIsEnabled(GL_BLEND);
    scissor = glIsEnabled(GL_SCISSOR_TEST);
    cull = glIsEnabled(GL_CULL_FACE);
    depth = glIsEnabled(GL_DEPTH_TEST);
  }

  void Restore() const
  {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(static_cast<GLuint>(program));
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
    glActiveTexture(static_cast<GLenum>(activeTexture));
    glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb), static_cast<GLenum>(blendDstRgb),
                        static_cast<GLenum>(blendSrcAlpha), static_cast<GLenum>(blendDstAlpha));
    const auto set = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
    set(GL_BLEND, blend);
    set(GL_SCISSOR_TEST, scissor);
    set(GL_CULL_FACE, cull);
    set(GL_DEPTH_TEST, depth);
  }
};

} // namespace

Renderer::~Renderer()
{
  Free(m_scene);
  Free(m_soft);
  Free(m_half);
  Free(m_quarter);
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao);
  for (GLuint program : {m_pointProgram, m_bandProgram, m_quadProgram})
    if (program)
      glDeleteProgram(program);
}

bool Renderer::Init()
{
  m_pointProgram = Link(kPointVertex, kPointFragment, m_error);
  m_bandProgram = m_pointProgram ? Link(kBandVertex, kBandFragment, m_error) : 0;
  m_quadProgram = m_bandProgram ? Link(kQuadVertex, kQuadFragment, m_error) : 0;
  if (!m_quadProgram)
    return false;
  m_pointTarget = glGetUniformLocation(m_pointProgram, "uTarget");
  m_bandTarget = glGetUniformLocation(m_bandProgram, "uTarget");
  m_quadTexture = glGetUniformLocation(m_quadProgram, "uTexture");
  m_quadGain = glGetUniformLocation(m_quadProgram, "uGain");
  GLfloat range[2] = {1.0f, 64.0f};
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
  m_maxPointSize = range[1];
  GLint vao = 0, buffer = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(static_cast<GLuint>(vao));
  glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
  return true;
}

bool Renderer::Ensure(Target& target, int width, int height)
{
  width = std::max(1, width);
  height = std::max(1, height);
  if (target.fbo && target.width == width && target.height == height)
    return true;
  Free(target);
  glGenTextures(1, &target.texture);
  glBindTexture(GL_TEXTURE_2D, target.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    m_error = "Framebuffer unvollständig";
    Free(target);
    return false;
  }
  target.width = width;
  target.height = height;
  return true;
}

void Renderer::Free(Target& target)
{
  if (target.fbo)
    glDeleteFramebuffers(1, &target.fbo);
  if (target.texture)
    glDeleteTextures(1, &target.texture);
  target = Target{};
}

void Renderer::DrawPoints(int targetWidth, int targetHeight)
{
  if (m_points.empty())
    return;
  glUseProgram(m_pointProgram);
  glUniform2f(m_pointTarget, static_cast<float>(targetWidth), static_cast<float>(targetHeight));
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_points.size() * sizeof(float)), m_points.data(),
               GL_STREAM_DRAW);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_points.size() / 6));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
}

void Renderer::DrawBands(int targetWidth, int targetHeight)
{
  if (m_bands.empty())
    return;
  glUseProgram(m_bandProgram);
  glUniform2f(m_bandTarget, static_cast<float>(targetWidth), static_cast<float>(targetHeight));
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_bands.size() * sizeof(float)), m_bands.data(),
               GL_STREAM_DRAW);
  const GLsizei stride = 6 * sizeof(float);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_bands.size() / 6));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

void Renderer::DrawTexture(GLuint texture, float gain)
{
  glUseProgram(m_quadProgram);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(m_quadTexture, 0);
  glUniform1f(m_quadGain, gain);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::AppendBand(const Annulus& band, float scale)
{
  float r0 = band.r0 * scale, r1 = band.r1 * scale;
  Color inner = band.inner, outer = band.outer;
  if (band.hairline && r1 - r0 < 1.0f)
  {
    // dünner als ein Pixel: 1 Pixel breit, Deckkraft entsprechend geringer (wie Canvas)
    const float coverage = std::max(0.0f, r1 - r0), mid = (r0 + r1) / 2.0f;
    r0 = mid - 0.5f;
    r1 = mid + 0.5f;
    inner.a *= coverage;
    outer.a *= coverage;
  }
  if (inner.a <= 0.0f && outer.a <= 0.0f)
    return;
  const float cx = band.cx * scale, cy = band.cy * scale, span = band.angle1 - band.angle0;
  const int segments = std::max(4, static_cast<int>(std::ceil(std::fabs(span) * std::max(r1, 8.0f) / 6.0f)));
  const auto push = [this](float x, float y, const Color& c) {
    m_bands.insert(m_bands.end(), {x, y, c.r, c.g, c.b, c.a});
  };
  for (int s = 0; s < segments; ++s)
  {
    const float a0 = band.angle0 + span * static_cast<float>(s) / segments;
    const float a1 = band.angle0 + span * static_cast<float>(s + 1) / segments;
    const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
    push(cx + c0 * r0, cy + s0 * r0, inner);
    push(cx + c0 * r1, cy + s0 * r1, outer);
    push(cx + c1 * r1, cy + s1 * r1, outer);
    push(cx + c0 * r0, cy + s0 * r0, inner);
    push(cx + c1 * r1, cy + s1 * r1, outer);
    push(cx + c1 * r0, cy + s1 * r0, inner);
  }
}

void Renderer::Draw(const DrawList& list)
{
  SavedState kodi;
  kodi.Save();
  const int width = kodi.viewport[2], height = kodi.viewport[3];
  if (width <= 0 || height <= 0 || !m_quadProgram)
    return;
  // Weiche Corona und Bloom in fester logischer Größe wie im Prototyp (Corona-Zelle 2, Glow 1/4),
  // damit Helligkeit und Glow-Radius nicht von der Bildschirmauflösung abhängen.
  const int logicalWidth = static_cast<int>(std::lround(list.width)), logicalHeight = static_cast<int>(std::lround(list.height));
  if (!Ensure(m_scene, width, height) || !Ensure(m_soft, logicalWidth / 2, logicalHeight / 2) ||
      !Ensure(m_half, logicalWidth / 2, logicalHeight / 2) || !Ensure(m_quarter, logicalWidth / 4, logicalHeight / 4))
  {
    kodi.Restore();
    return;
  }
  const float scale = static_cast<float>(height) / list.height; // logisch -> Gerätepixel

  glBindVertexArray(m_vao);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  // 1. Weiche Corona in halber Auflösung
  glBindFramebuffer(GL_FRAMEBUFFER, m_soft.fbo);
  glViewport(0, 0, m_soft.width, m_soft.height);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  m_points.clear();
  const float softScale = 0.5f; // logisch -> Corona-Zellen
  for (const auto& d : list.soft)
  {
    const float x = std::floor(d.x * softScale), y = std::floor(d.y * softScale);
    if (x < 0.0f || y < 0.0f || x >= m_soft.width || y >= m_soft.height)
      continue;
    m_points.insert(m_points.end(), {x + 0.5f, y + 0.5f, 1.0f, d.r, d.g, d.b});
  }
  glBlendFunc(GL_ONE, GL_ONE);
  DrawPoints(m_soft.width, m_soft.height);

  // 2. Szene in voller Auflösung
  glBindFramebuffer(GL_FRAMEBUFFER, m_scene.fbo);
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);
  m_bands.clear();
  for (const auto& band : list.under)
    AppendBand(band, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  DrawBands(width, height);
  glBlendFunc(GL_ONE, GL_ONE);
  DrawTexture(m_soft.texture, 1.0f);
  m_points.clear();
  const PixelGrid grid = ComputePixelGrid(list.pixel, scale);
  const float gain = list.pixelGain * grid.alpha;
  for (const auto& d : list.dust)
  {
    const float size = std::min(m_maxPointSize, static_cast<float>(grid.grid) * d.size);
    const float x = std::floor(d.x * scale / grid.grid) * grid.grid;
    const float y = std::floor(d.y * scale / grid.grid) * grid.grid;
    if (x < 0.0f || y < 0.0f || x >= width || y >= height)
      continue;
    m_points.insert(m_points.end(), {x + size / 2.0f, y + size / 2.0f, size, d.r * gain, d.g * gain, d.b * gain});
  }
  DrawPoints(width, height);
  m_bands.clear();
  for (const auto& band : list.over)
    AppendBand(band, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  DrawBands(width, height);

  // 3. Bloom: zweimal halbieren
  glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, m_half.fbo);
  glViewport(0, 0, m_half.width, m_half.height);
  DrawTexture(m_scene.texture, 1.0f);
  glBindFramebuffer(GL_FRAMEBUFFER, m_quarter.fbo);
  glViewport(0, 0, m_quarter.width, m_quarter.height);
  DrawTexture(m_half.texture, 1.0f);

  // 4. In Kodis Framebuffer zusammensetzen
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(kodi.framebuffer));
  glViewport(kodi.viewport[0], kodi.viewport[1], width, height);
  if (kodi.scissor)
    glEnable(GL_SCISSOR_TEST);
  DrawTexture(m_scene.texture, 1.0f);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  DrawTexture(m_quarter.texture, list.bloom);

  // 5. Schwarzes Loch und weißer Rand, deckend
  m_bands.clear();
  AppendBand({list.holeX, list.holeY, 0.0f, list.holeRadius, 0.0f, 6.283185307f, {0.0f, 0.0f, 0.0f, 1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}, false},
             scale);
  AppendBand(list.holeRing, scale);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  DrawBands(width, height);

  kodi.Restore();
}

} // namespace spektra
