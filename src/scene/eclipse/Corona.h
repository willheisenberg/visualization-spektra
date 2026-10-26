#pragma once

#include "Context.h"
#include "Flame.h"
#include "scene/DrawList.h"
#include "scene/Random.h"

#include <array>
#include <cstddef>
#include <vector>

namespace spektra
{

// Warme Corona aus Staub: goldener Saum, 9 Streamer, Magnetbögen. Kommt mit der Melodie, geht ohne sie.
class Corona
{
public:
  explicit Corona(Random& rng);

  void Strike(float angle, float strength);
  void Drop(float corona);
  void Step(const Context& c, float hold, const Flame& flame, float birth, DrawList& out);

  size_t Size() const { return m_particles.size(); }
  float Level() const { return m_level; }

private:
  struct Streamer
  {
    float a = 0.0f, w = 0.0f, warm = 0.0f, len = 0.0f, curve = 0.0f, lvl = 0.0f;
  };
  struct Loop
  {
    float a, span, h, age, life;
  };
  enum Kind : int
  {
    kRim = 0,
    kStreamer = 1,
    kLoop = 2
  };
  struct Particle
  {
    int kind;
    float x, y, vx, vy, life, max, b;
    float warm, curve;                  // Streamer
    float loopA, loopSpan, loopH, strand, off; // Bogen
  };

  void Reseed(Streamer& s, float angle);
  bool Room(const Context& c) const { return static_cast<int>(m_particles.size()) < c.maxParticles; }

  Random& m_rng;
  std::array<Streamer, 9> m_streamers{};
  std::vector<Loop> m_loops;
  std::vector<Particle> m_particles;
  float m_level = 0.0f;
};

} // namespace spektra
