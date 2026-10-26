#include "scene/DrawList.h"
#include "scene/PixelGrid.h"
#include "scene/Random.h"
#include "scene/eclipse/Context.h"
#include "scene/eclipse/Palette.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

TEST(PaletteTest, FireColorUsesPrototypeSteps)
{
  const Rgb white = FireColor(0.0f);
  EXPECT_FLOAT_EQ(white.r, 250.0f);
  EXPECT_FLOAT_EQ(white.b, 255.0f);
  // h = 1: min(0,999; h) · 63 = 62,94 -> Stufe 62 (wie RGB[(…)|0] im Prototyp): 62/63 · 4 = 3,937
  const Rgb top = FireColor(1.0f);
  EXPECT_NEAR(top.r, 214.0f + (190.0f - 214.0f) * (62.0f / 63.0f * 4.0f - 3.0f), 0.01f);
  // Stufe 31: 31/63 · 4 = 1,968 -> Silber nach Gold, Anteil 0,968
  const Rgb mid = FireColor(0.5f);
  EXPECT_NEAR(mid.g, 230.0f + (182.0f - 230.0f) * (31.0f / 63.0f * 4.0f - 1.0f), 0.01f);
}

TEST(RandomTest, SameSeedSameSequence)
{
  Random a(7), b(7);
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(a.Uniform(), b.Uniform());
}

TEST(RandomTest, UniformStaysInRange)
{
  Random r(3);
  double sum = 0.0;
  for (int i = 0; i < 100000; ++i)
  {
    const float v = r.Uniform();
    ASSERT_GE(v, 0.0f);
    ASSERT_LT(v, 1.0f);
    sum += v;
  }
  EXPECT_NEAR(sum / 100000.0, 0.5, 0.01);
}

TEST(RandomTest, SpawnCountAveragesTheRate)
{
  Random r(5);
  long total = 0;
  for (int i = 0; i < 100000; ++i)
    total += r.SpawnCount(2.3f);
  EXPECT_NEAR(total / 100000.0, 2.3, 0.02);
  EXPECT_EQ(r.SpawnCount(0.0f), 0);
}

TEST(FlowTest, FieldIsDivergenceFree)
{
  const float h = 1e-3f;
  for (float u : {-1.5f, 0.2f, 2.0f})
    for (float v : {-0.7f, 0.9f})
    {
      float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
      Flow(u + h, v, 1.3f, fx1, fy1);
      Flow(u - h, v, 1.3f, fx2, fy2);
      Flow(u, v + h, 1.3f, fx3, fy3);
      Flow(u, v - h, 1.3f, fx4, fy4);
      const float divergence = (fx1 - fx2) / (2 * h) + (fy3 - fy4) / (2 * h);
      EXPECT_NEAR(divergence, 0.0f, 0.05f) << u << "/" << v;
    }
}

TEST(AngleTest, DistanceWrapsAround)
{
  EXPECT_NEAR(AngleDistance(0.1f, kTau - 0.1f), 0.2f, 1e-4f);
  EXPECT_NEAR(AngleDistance(kPi, 0.0f), kPi, 1e-4f);
}

TEST(PixelGridTest, BlocksAndFineDust)
{
  const auto normal = ComputePixelGrid(1.0f, 1.5f);
  EXPECT_EQ(normal.grid, 2);
  EXPECT_FLOAT_EQ(normal.alpha, 1.0f);
  const auto fine = ComputePixelGrid(0.5f, 1.5f);
  EXPECT_EQ(fine.grid, 1);
  EXPECT_FLOAT_EQ(fine.alpha, 0.5625f);
  EXPECT_EQ(ComputePixelGrid(8.0f, 1.5f).grid, 12);
  EXPECT_FLOAT_EQ(PixelGain(1.0f), 1.0f);
  EXPECT_FLOAT_EQ(PixelGain(2.0f), std::pow(2.0f, -1.1f));
  EXPECT_FLOAT_EQ(PixelGain(0.5f), std::pow(0.5f, -0.8f));
}

TEST(DrawListTest, DustIsPremultiplied)
{
  const Dust d = MakeDust(1.0f, 2.0f, 2, Rgb{255.0f, 127.5f, 0.0f}, 0.5f);
  EXPECT_FLOAT_EQ(d.r, 0.5f);
  EXPECT_FLOAT_EQ(d.g, 0.25f);
  EXPECT_FLOAT_EQ(d.b, 0.0f);
  EXPECT_FLOAT_EQ(d.size, 2.0f);
  const Annulus l = Line(10.0f, 20.0f, 100.0f, 2.0f, Rgb{255.0f, 255.0f, 255.0f}, 0.4f);
  EXPECT_FLOAT_EQ(l.r0, 99.0f);
  EXPECT_FLOAT_EQ(l.r1, 101.0f);
  EXPECT_TRUE(l.hairline);
  EXPECT_FLOAT_EQ(l.inner.a, 0.4f);
}
