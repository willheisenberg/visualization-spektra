#include "EclipseScene.h"

#include "Palette.h"
#include "core/Routing.h"
#include "scene/PixelGrid.h"

#include <algorithm>
#include <cmath>

namespace spektra
{

EclipseScene::EclipseScene(uint64_t seed, double startTime)
    : m_rng(seed), m_dust(m_rng), m_flame(m_rng), m_corona(m_rng), m_t(startTime)
{
}

void EclipseScene::SetViewport(float width, float height)
{
  if (width > 0.0f && height > 0.0f)
  {
    m_width = width;
    m_height = height;
  }
}

float EclipseScene::MelodyAngle(float centroid)
{
  const float a = kPi / 2.0f - centroid * kPi;
  return m_rng.Uniform() < 0.5f ? a : kPi - a;
}

namespace
{
// Phasen für die float-Rechnung: stündlich umlaufend, damit sin/cos auch nach Tagen genau bleiben.
// Der Sprung beim Umlauf verschiebt nur Rauschmuster und Strömung einmal pro Stunde.
float Phase(double t)
{
  return static_cast<float>(std::fmod(t, 3600.0));
}
} // namespace

void EclipseScene::Frame(const Features& f, const Settings& s, float dt, DrawList& out)
{
  out.Clear();
  out.width = m_width;
  out.height = m_height;
  m_t += dt;

  Context c;
  c.t = Phase(m_t);
  c.dt = dt;
  c.width = m_width;
  c.height = m_height;
  c.cx = m_width / 2.0f;
  c.cy = m_height / 2.0f;
  c.M = static_cast<float>(s.intensity) / 100.0f;
  c.flame = static_cast<float>(s.flame) / 100.0f;
  c.waves = static_cast<float>(s.waves) / 100.0f;
  c.particles = static_cast<float>(s.particles) / 100.0f;
  c.corona = static_cast<float>(s.corona) / 100.0f;
  c.maxParticles = s.maxParticles;

  const float loud = f.loudness;
  const Rgb tint = Mix(kSilver, kGold, 0.15f + 0.35f * loud);
  const float onWave = RoutedOnset(s, f, kWave), onBurst = RoutedOnset(s, f, kBurst);
  const float onJet = RoutedOnset(s, f, kJet), onStreamer = RoutedOnset(s, f, kStreamer);
  const float onDust = RoutedOnset(s, f, kDust);
  const bool drop = s.drop && f.drop;

  // Anschläge: Welle, Flammenstoß rundum
  if (onWave > 0.05f)
  {
    m_beatT = m_t;
    if (c.waves > 0.0f)
      m_waves.push_back({m_t, std::max(0.2f, onWave) * c.waves});
  }
  if (onBurst > 0.05f)
  {
    m_beatT = m_t;
    m_flame.PulseProfile(0.45f + 0.8f * onBurst);
  }
  const float birth = static_cast<float>(std::exp(-(m_t - m_beatT) * 3.5));
  c.R = m_height * 0.19f *
        (1.0f + 0.05f * RoutedLevel(s, f, kBreath) + 0.035f * birth * c.waves + 0.012f * std::sin(c.t * kTau / 9.0f));

  // Stichflammen
  if (onJet > 0.05f)
    m_flame.PulseProfile(0.35f + 0.6f * onJet, 1, 2, 0.12f, 0.0f);

  // Drop: alles zugleich
  if (drop)
  {
    m_flame.PulseProfile(1.5f, 6, 9, 0.6f, 0.4f);
    if (c.waves > 0.0f)
    {
      m_waves.push_back({m_t, c.waves});
      m_waves.push_back({m_t + 0.25, 0.8f * c.waves});
    }
    m_corona.Drop(c.corona);
    m_dust.Release(c, 1.0f, 6);
  }
  while (!m_waves.empty() && m_t - m_waves.front().t0 > 7.0)
    m_waves.erase(m_waves.begin());

  // Flamme: Grundhöhe aus dem Pegel, Unregelmäßigkeit aus dem Feinspektrum
  m_flame.Decay(dt);
  m_flame.Emit(c, RoutedLevel(s, f, kFlame), RoutedLevel(s, f, kFree), loud, f.fine, m_dust);
  if (onDust > 0.05f)
    m_dust.Release(c, onDust, 1 + static_cast<int>(onDust * 3.0f));

  // Wellen: anfangs über einen Lichtschleier mit dem Kern verbunden
  for (const auto& w : m_waves)
  {
    const float age = static_cast<float>(m_t - w.t0);
    if (age < 0.0f)
      continue;
    const float k = age / 7.0f, rr = c.R * (1.0f + age * 0.2f + age * age * 0.025f);
    const float link = std::max(0.0f, 1.0f - age / 1.6f);
    if (link > 0.0f)
    {
      const Color inner{tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, 0.09f * link * w.s * c.waves};
      Color outer = inner;
      outer.a = 0.015f * link * w.s * c.waves;
      out.under.push_back({c.cx, c.cy, c.R, rr, 0.0f, kTau, inner, outer, false});
    }
    out.under.push_back(Line(c.cx, c.cy, rr, 1.0f - 0.5f * k, tint, 0.45f * w.s * c.waves * (1.0f - k) * (1.0f - k)));
  }

  // Strömung
  const float flowLevel = RoutedLevel(s, f, kFlow);
  m_flowT += dt * (0.4f + 1.6f * flowLevel);
  c.flowT = Phase(m_flowT);
  c.flowStrength = c.R * (0.3f + 0.55f * flowLevel);
  m_dust.Step(c, out);

  // Corona: Pegel hält sie am Leben, Anschläge setzen Streamer an die Stelle der Tonhöhe
  if (onStreamer > 0.05f && c.corona > 0.0f)
    m_corona.Strike(MelodyAngle(f.centroid), onStreamer);
  m_corona.Step(c, RoutedLevel(s, f, kCorona), m_flame, birth, out);

  // Kernringe und Glutfuß
  out.over.push_back(Line(c.cx, c.cy, c.R, 1.8f, kWhite, 0.95f));
  out.over.push_back(Line(c.cx, c.cy, c.R * (1.014f + 0.006f * std::sin(c.t * 0.5f + 1.0f)), 0.6f, tint, 0.22f));
  for (int i = 0; i < Flame::kCells; ++i)
  {
    const float e = std::max(0.0f, m_flame.Heat(i) - 0.35f);
    if (e < 0.02f)
      continue;
    const float a0 = static_cast<float>(i) / Flame::kCells * kTau;
    const float a1 = (static_cast<float>(i) + 1.2f) / Flame::kCells * kTau;
    Annulus arc = Arc(c.cx, c.cy, c.R, 3.0f, a0, a1, kGold, std::min(0.7f, e * 0.8f));
    arc.hairline = false;
    out.over.push_back(arc);
  }

  out.bloom = 0.5f + 0.35f * loud;
  out.holeX = c.cx;
  out.holeY = c.cy;
  out.holeRadius = c.R * 0.992f;
  out.holeRing = Line(c.cx, c.cy, c.R, 1.8f, kWhite, 0.95f);
  out.pixel = kPixelSteps[std::min(8, std::max(0, s.pixel))];
  out.pixelGain = PixelGain(out.pixel);
}

} // namespace spektra
