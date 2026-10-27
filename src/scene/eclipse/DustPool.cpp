#include "DustPool.h"

#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

void DustPool::Release(const Context& c, float strength, int spots)
{
  if (c.particles <= 0.0f)
    return;
  for (int s = 0; s < spots; ++s)
  {
    const float a0 = m_rng.Uniform() * kTau;
    const float n = std::floor(60.0f + 260.0f * strength) * c.particles;
    for (int j = 0; static_cast<float>(j) < n && static_cast<int>(m_grains.size()) < c.maxParticles; ++j)
    {
      const float a = a0 + m_rng.Gauss() * 0.08f, r0 = c.R * 1.01f;
      const float speed = c.R * (0.4f + m_rng.Uniform() * 0.9f) * (0.6f + strength);
      const float life = (2.0f + m_rng.Uniform() * 3.0f) * c.reach;
      m_grains.push_back({c.cx + std::cos(a) * r0, c.cy + std::sin(a) * r0, std::cos(a) * speed,
                          std::sin(a) * speed, life, life, true, m_rng.Uniform() < 0.9f ? 1 : 2});
    }
  }
}

void DustPool::Step(const Context& c, DrawList& out)
{
  const float dragBound = std::pow(0.986f, c.dt * 60.0f);
  // Rückwärts mit Tausch ans Ende: wie im Prototyp, der getauschte Eintrag ist schon bearbeitet.
  for (size_t i = m_grains.size(); i-- > 0;)
  {
    Grain& p = m_grains[i];
    p.life -= c.dt;
    const float dx = p.x - c.cx, dy = p.y - c.cy;
    float d = std::hypot(dx, dy);
    if (d == 0.0f)
      d = 1.0f;
    const float nx = dx / d, ny = dy / d;
    const float regulator = p.free ? c.particles : c.flame;
    if (p.life <= 0.0f || d < c.R || p.x < 0.0f || p.y < 0.0f || p.x >= c.width || p.y >= c.height ||
        regulator == 0.0f)
    {
      m_grains[i] = m_grains.back();
      m_grains.pop_back();
      continue;
    }
    const float u = dx / c.R, v = dy / c.R;
    float fx, fy;
    Flow(u, v, c.flowT, fx, fy);
    if (p.free)
    {
      // freie Körner folgen der Strömung und driften langsam nach außen
      const float drift = c.R * 0.18f * c.reach;
      const float tx = fx * c.flowStrength + nx * drift, ty = fy * c.flowStrength + ny * drift;
      p.vx += (tx - p.vx) * 1.6f * c.dt;
      p.vy += (ty - p.vy) * 1.6f * c.dt;
    }
    else
    {
      // gebundene: Auftrieb nach außen, Sog zurück, seitlich von der Strömung geschwenkt
      p.vx += fx * c.flowStrength * 1.2f * c.dt - nx * c.R * 0.45f * c.dt;
      p.vy += fy * c.flowStrength * 1.2f * c.dt - ny * c.R * 0.45f * c.dt;
      p.vx *= dragBound;
      p.vy *= dragBound;
    }
    p.x += p.vx * c.dt;
    p.y += p.vy * c.dt;
    const float k = 1.0f - p.life / p.max;
    const float h = p.free ? 0.35f + k * 0.6f : std::min(1.0f, (d - c.R) / (c.R * 0.9f)) * 0.75f + k * 0.25f;
    const float alpha = p.free ? std::min(1.0f, p.life) * 0.55f * c.particles : (1.0f - k * k) * 0.75f * c.flame;
    out.dust.push_back(MakeDust(p.x, p.y, p.size, FireColor(PaletteFor(c.palette), h), alpha));
  }
}

} // namespace spektra
