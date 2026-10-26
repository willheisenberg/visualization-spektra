#include "scene/eclipse/Corona.h"
#include "scene/eclipse/Flame.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
struct Fixture
{
  Random rng{11};
  Flame flame{rng};
  Corona corona{rng};
  Context c;
  DrawList out;

  void Run(float hold, float seconds)
  {
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i)
    {
      c.t += c.dt;
      out.Clear();
      corona.Step(c, hold, flame, 0.0f, out);
    }
  }
};
} // namespace

TEST(CoronaTest, ComesWithMelodyAndFadesWithoutIt)
{
  Fixture f;
  f.corona.Strike(0.0f, 1.0f);
  f.Run(0.8f, 2.0f);
  const size_t peak = f.out.soft.size();
  EXPECT_GT(peak, 500u);
  EXPECT_GT(f.corona.Level(), 0.5f);
  f.Run(0.0f, 15.0f);
  EXPECT_LT(f.corona.Level(), 0.1f);
  EXPECT_LT(f.out.soft.size(), peak / 4);
}

TEST(CoronaTest, RegulatorZeroClearsEverything)
{
  Fixture f;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 1.0f);
  ASSERT_GT(f.corona.Size(), 0u);
  f.c.corona = 0.0f;
  f.Run(1.0f, 0.1f);
  EXPECT_EQ(f.corona.Size(), 0u);
  EXPECT_TRUE(f.out.soft.empty());
  EXPECT_TRUE(f.out.dust.empty());
}

TEST(CoronaTest, StrikeGrowsAStreamerWhereTheMelodyIs)
{
  Fixture f;
  f.corona.Strike(kPi / 2.0f, 1.0f); // unten (y wächst nach unten)
  f.Run(0.5f, 1.5f);
  int below = 0, above = 0;
  for (const auto& d : f.out.soft)
  {
    if (d.y > f.c.cy + f.c.R * 1.5f)
      ++below;
    if (d.y < f.c.cy - f.c.R * 1.5f)
      ++above;
  }
  EXPECT_GT(below, 4 * (above + 1));
}

TEST(CoronaTest, LimitHolds)
{
  Fixture f;
  f.c.maxParticles = 500;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 2.0f);
  EXPECT_LE(f.corona.Size(), 500u);
}

TEST(CoronaTest, NothingInsideTheHole)
{
  Fixture f;
  f.corona.Drop(1.0f);
  f.Run(1.0f, 2.0f);
  for (const auto& d : f.out.soft)
    EXPECT_GE(std::hypot(d.x - f.c.cx, d.y - f.c.cy), f.c.R * 0.95f);
}
