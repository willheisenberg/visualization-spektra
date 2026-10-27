#include "Corona.h"

#include "Palette.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

Corona::Corona(Random& rng) : m_rng(rng)
{
  for (auto& s : m_streamers)
    Reseed(s, m_rng.Uniform() * kTau);
}

void Corona::Reseed(Streamer& s, float angle)
{
  s.a = angle;
  s.w = 0.04f + m_rng.Uniform() * 0.16f;
  s.warm = m_rng.Uniform();
  s.len = 1.3f + m_rng.Uniform() * 2.6f;
  s.curve = (m_rng.Uniform() - 0.5f) * 0.25f;
}

void Corona::Strike(float angle, float strength)
{
  // schwächsten Streamer an der Stelle der Tonhöhe neu einsetzen
  auto weakest = std::min_element(m_streamers.begin(), m_streamers.end(),
                                  [](const Streamer& a, const Streamer& b) { return a.lvl < b.lvl; });
  Reseed(*weakest, angle + m_rng.Gauss() * 0.15f);
  weakest->lvl = std::min(1.4f, 0.5f + strength);
  if (m_rng.Uniform() < 0.45f)
    m_loops.push_back({angle + m_rng.Gauss() * 0.2f, 0.07f + m_rng.Uniform() * 0.2f,
                       0.08f + m_rng.Uniform() * 0.22f, 0.0f, 2.5f + m_rng.Uniform() * 3.0f});
}

void Corona::Drop(float corona)
{
  m_level = corona;
  for (auto& s : m_streamers)
  {
    Reseed(s, m_rng.Uniform() * kTau);
    s.lvl = 1.2f * corona;
  }
}

void Corona::Step(const Context& c, float hold, const Flame& flame, float birth, DrawList& out)
{
  const Palette& pal = PaletteFor(c.palette);
  const float target = (0.04f * c.M + 0.9f * hold) * c.corona;
  m_level += (target - m_level) * (1.0f - std::exp(-c.dt / (hold > m_level ? 0.6f : 2.5f)));
  if (c.corona == 0.0f)
    m_level = 0.0f;
  for (auto& s : m_streamers)
  {
    s.lvl *= std::exp(-c.dt / (1.2f + 4.0f * hold));
    s.a += s.curve * 0.02f * c.dt;
  }

  // Saum am Rand: dicht, golden, ungleichmäßig (stärker unter aktiven Streamern)
  int n = m_rng.SpawnCount((300.0f * c.corona * c.M + 5600.0f * m_level) * c.dt);
  for (int j = 0; j < n && Room(c); ++j)
  {
    const Streamer& s = m_streamers[static_cast<size_t>(m_rng.Uniform() * m_streamers.size())];
    const float a = m_rng.Uniform() < 0.55f ? s.a + m_rng.Gauss() * s.w * 3.0f : m_rng.Uniform() * kTau;
    const float r = c.R * (1.004f + std::pow(m_rng.Uniform(), 3.0f) * 0.14f);
    const float life = 0.5f + m_rng.Uniform();
    Particle p{};
    p.kind = kRim;
    p.x = c.cx + std::cos(a) * r;
    p.y = c.cy + std::sin(a) * r;
    p.vx = -std::sin(a) * c.R * m_rng.Gauss() * 0.05f;
    p.vy = std::cos(a) * c.R * m_rng.Gauss() * 0.05f;
    p.life = p.max = life;
    p.b = 0.3f + 0.7f * std::min(1.0f, m_level + s.lvl * 0.5f);
    m_particles.push_back(p);
  }
  // Streamer: Fasern ziehen weit nach außen
  for (const auto& s : m_streamers)
  {
    const float vis = s.lvl * (0.3f + 0.7f * m_level) * c.corona;
    if (vis < 0.02f)
      continue;
    n = m_rng.SpawnCount(1500.0f * vis * c.dt);
    for (int j = 0; j < n && Room(c); ++j)
    {
      const float a = s.a + m_rng.Gauss() * s.w, r = c.R * 1.01f;
      const float speed = c.R * (0.35f + m_rng.Uniform() * 0.7f);
      const float life = s.len * (0.5f + m_rng.Uniform() * 0.7f) * c.R / speed;
      Particle p{};
      p.kind = kStreamer;
      p.x = c.cx + std::cos(a) * r;
      p.y = c.cy + std::sin(a) * r;
      p.vx = std::cos(a) * speed;
      p.vy = std::sin(a) * speed;
      p.life = p.max = life;
      p.b = vis;
      p.warm = s.warm;
      p.curve = s.curve;
      m_particles.push_back(p);
    }
  }
  // Magnetbögen: Staub läuft vom Rand über einen Bogen zurück zum Rand
  for (size_t i = m_loops.size(); i-- > 0;)
  {
    Loop& lp = m_loops[i];
    lp.age += c.dt;
    if (lp.age > lp.life)
    {
      m_loops.erase(m_loops.begin() + static_cast<long>(i));
      continue;
    }
    const float env = std::sin(lp.age / lp.life * kPi);
    n = m_rng.SpawnCount(700.0f * env * c.dt);
    for (int j = 0; j < n && Room(c); ++j)
    {
      Particle p{};
      p.kind = kLoop;
      p.life = p.max = 1.2f + m_rng.Uniform() * 1.2f;
      p.b = env;
      p.loopA = lp.a;
      p.loopSpan = lp.span;
      p.loopH = lp.h;
      p.strand = 0.75f + m_rng.Uniform() * 0.5f;
      p.off = m_rng.Gauss() * 0.01f;
      m_particles.push_back(p);
    }
  }

  if (c.corona == 0.0f)
  {
    m_particles.clear();
    m_loops.clear();
  }
  const float birthGlow = 1.0f + 0.5f * birth;
  const auto soft = [&](float x, float y, const Rgb& col, float al) {
    out.soft.push_back(MakeDust(x, y, 1, col, al * c.corona));
  };
  const auto sharp = [&](float x, float y, const Rgb& col, float al) {
    out.dust.push_back(MakeDust(x, y, 1, col, al * c.corona));
  };
  for (size_t i = m_particles.size(); i-- > 0;)
  {
    Particle& p = m_particles[i];
    p.life -= c.dt;
    if (p.life <= 0.0f)
    {
      m_particles[i] = m_particles.back();
      m_particles.pop_back();
      continue;
    }
    const float k = 1.0f - p.life / p.max;
    if (p.kind == kLoop)
    {
      const float a = p.loopA - p.loopSpan / 2.0f + p.loopSpan * k + p.off;
      const float r = c.R * (1.0f + p.loopH * p.strand * std::sin(kPi * k));
      p.x = c.cx + std::cos(a) * r;
      p.y = c.cy + std::sin(a) * r;
      const float al = (0.5f * p.b * std::sin(kPi * k) + 0.1f) * birthGlow;
      soft(p.x, p.y, pal.coronaNear, al * 0.6f);
      sharp(p.x, p.y, pal.coronaNear, al * 0.5f);
      continue;
    }
    const float dx = p.x - c.cx, dy = p.y - c.cy;
    float d = std::hypot(dx, dy);
    if (d == 0.0f)
      d = 1.0f;
    if (d < c.R)
    {
      m_particles[i] = m_particles.back();
      m_particles.pop_back();
      continue;
    }
    const float nx = dx / d, ny = dy / d, u = dx / c.R, v = dy / c.R;
    float fx, fy;
    Flow(u, v, c.flowT, fx, fy);
    const float heat = flame.HeatAt(std::atan2(dy, dx));
    if (p.kind == kStreamer)
    {
      // radial, leicht gebogen, von Strömung und Flammenhitze geschwenkt
      const float push = c.R * 0.3f * std::max(0.0f, heat - 0.5f);
      p.vx += (fx * c.flowStrength * 0.25f - ny * p.curve * c.R * 0.3f) * c.dt + nx * push * c.dt;
      p.vy += (fy * c.flowStrength * 0.25f + nx * p.curve * c.R * 0.3f) * c.dt + ny * push * c.dt;
    }
    else
    {
      p.vx += fx * c.flowStrength * 0.15f * c.dt;
      p.vy += fy * c.flowStrength * 0.15f * c.dt;
    }
    p.x += p.vx * c.dt;
    p.y += p.vy * c.dt;
    const float rr = d / c.R - 1.0f;
    if (p.kind == kRim)
    {
      const float al = 0.55f * p.b * std::sin(kPi * k) * std::exp(-rr * 9.0f) * birthGlow;
      soft(p.x, p.y, pal.coronaNear, al);
      sharp(p.x, p.y, pal.coronaNear, al * 0.35f);
    }
    else
    {
      const float fade = std::sin(kPi * std::min(1.0f, k * 1.15f)) * std::exp(-rr * 0.55f);
      const float far = std::min(1.0f, rr / 2.2f);
      const Rgb col = far < 0.35f ? Mix(pal.coronaNear, pal.coronaMid, far / 0.35f)
                                  : Mix(pal.coronaMid, p.warm > 0.5f ? pal.coronaMid : pal.coronaFar,
                                        (far - 0.35f) / 0.65f);
      const float al = 0.16f * p.b * fade;
      soft(p.x, p.y, col, al);
      if (m_rng.Uniform() < 0.15f)
        sharp(p.x, p.y, col, al * 0.8f);
    }
  }
}

} // namespace spektra
