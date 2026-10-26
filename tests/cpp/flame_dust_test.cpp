#include "scene/eclipse/DustPool.h"
#include "scene/eclipse/Flame.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
std::array<float, kFineBands> Fine(float v)
{
  std::array<float, kFineBands> f{};
  f.fill(v);
  return f;
}
} // namespace

TEST(FlameTest, EmitsAllAroundTheRing)
{
  Random rng(1);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  for (int i = 0; i < 30; ++i)
  {
    c.t += c.dt;
    flame.Decay(c.dt);
    flame.Emit(c, 0.6f, 0.0f, 0.5f, Fine(0.4f), pool);
  }
  int quadrant[4] = {};
  for (const auto& g : pool.Grains())
    ++quadrant[(g.x >= c.cx ? 1 : 0) + (g.y >= c.cy ? 2 : 0)];
  for (int q : quadrant)
    EXPECT_GT(q, 20);
}

TEST(FlameTest, FlameRegulatorZeroEmitsNothing)
{
  Random rng(1);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  c.flame = 0.0f;
  flame.PulseProfile(1.5f);
  for (int i = 0; i < 30; ++i)
    flame.Emit(c, 1.0f, 1.0f, 1.0f, Fine(1.0f), pool);
  EXPECT_EQ(pool.Size(), 0u);
}

TEST(FlameTest, PulseRaisesHeat)
{
  Random rng(9);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  flame.Emit(c, 0.0f, 0.0f, 0.0f, Fine(0.0f), pool);
  float before = 0.0f;
  for (int i = 0; i < Flame::kCells; ++i)
    before += flame.Heat(i);
  flame.PulseProfile(1.0f);
  flame.Emit(c, 0.0f, 0.0f, 0.0f, Fine(0.0f), pool);
  float after = 0.0f;
  for (int i = 0; i < Flame::kCells; ++i)
    after += flame.Heat(i);
  EXPECT_GT(after, before * 3.0f);
  EXPECT_FLOAT_EQ(flame.HeatAt(0.0f), flame.Heat(0));
  EXPECT_FLOAT_EQ(flame.HeatAt(kPi), flame.Heat(Flame::kCells / 2));
}

TEST(FlameTest, EmissionThinsOutAtTheLimit)
{
  Random rng(4);
  Flame flame(rng);
  DustPool pool(rng);
  Context c;
  c.maxParticles = 500;
  flame.PulseProfile(1.5f);
  for (int i = 0; i < 120; ++i)
    flame.Emit(c, 1.0f, 1.0f, 1.0f, Fine(1.0f), pool);
  EXPECT_LE(pool.Size(), 500u);
}

TEST(DustPoolTest, NothingStaysInsideTheRing)
{
  Random rng(2);
  DustPool pool(rng);
  Context c;
  pool.Add({c.cx + c.R * 0.5f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  pool.Add({c.cx + c.R * 1.2f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  DrawList out;
  pool.Step(c, out);
  EXPECT_EQ(pool.Size(), 1u);
  EXPECT_EQ(out.dust.size(), 1u);
}

TEST(DustPoolTest, RegulatorsRemoveTheirGrainsAtOnce)
{
  Random rng(2);
  DustPool pool(rng);
  Context c;
  pool.Add({c.cx + c.R * 1.5f, c.cy, 0.0f, 0.0f, 3.0f, 3.0f, true, 1});
  pool.Add({c.cx + c.R * 1.2f, c.cy, 0.0f, 0.0f, 1.0f, 1.0f, false, 1});
  c.particles = 0.0f;
  DrawList out;
  pool.Step(c, out);
  ASSERT_EQ(pool.Size(), 1u);
  EXPECT_FALSE(pool.Grains()[0].free);
  c.flame = 0.0f;
  out.Clear();
  pool.Step(c, out);
  EXPECT_EQ(pool.Size(), 0u);
  EXPECT_TRUE(out.dust.empty());
}

TEST(DustPoolTest, ReleaseRespectsTheLimit)
{
  Random rng(6);
  DustPool pool(rng);
  Context c;
  c.maxParticles = 100;
  pool.Release(c, 1.0f, 6);
  EXPECT_EQ(pool.Size(), 100u);
  for (const auto& g : pool.Grains())
    EXPECT_TRUE(g.free);
}

TEST(DustPoolTest, ReleaseDoesNothingWithoutParticles)
{
  Random rng(6);
  DustPool pool(rng);
  Context c;
  c.particles = 0.0f;
  pool.Release(c, 1.0f, 6);
  EXPECT_EQ(pool.Size(), 0u);
}
