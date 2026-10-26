#include "scene/eclipse/EclipseScene.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace spektra;

namespace
{
Features Loud()
{
  Features f;
  f.level = {0.7f, 0.7f, 0.7f, 0.7f};
  f.loudness = 0.6f;
  f.centroid = 0.5f;
  f.fine.fill(0.5f);
  return f;
}

struct SceneRun
{
  EclipseScene scene{42};
  Settings settings = Settings::Defaults();
  DrawList out;

  // n Bilder; alle `every` Bilder (0 = nie) schlagen die Bänder in `bands` an.
  void Frames(int n, const Features& base, int every = 0, std::initializer_list<int> bands = {kBass, kLow, kMid, kHigh})
  {
    for (int i = 0; i < n; ++i)
    {
      Features f = base;
      if (every > 0 && i % every == 0)
        for (int b : bands)
          f.onset[static_cast<size_t>(b)] = 0.8f;
      scene.Frame(f, settings, 1.0f / 60.0f, out);
    }
  }
};
} // namespace

TEST(EclipseSceneTest, RingSitsInTheCenter)
{
  SceneRun r;
  r.scene.SetViewport(1280.0f, 720.0f);
  r.Frames(1, Features{});
  EXPECT_FLOAT_EQ(r.out.holeX, 640.0f);
  EXPECT_FLOAT_EQ(r.out.holeY, 360.0f);
  EXPECT_NEAR(r.out.holeRadius, 0.19f * 720.0f * 0.992f, 0.03f * 0.19f * 720.0f);
}

TEST(EclipseSceneTest, WideViewportKeepsRingCentered)
{
  SceneRun r;
  r.scene.SetViewport(1680.0f, 720.0f); // 21:9
  r.Frames(60, Loud(), 20);
  EXPECT_FLOAT_EQ(r.out.holeX, 840.0f);
  EXPECT_FLOAT_EQ(r.out.width, 1680.0f);
  EXPECT_NEAR(r.out.holeRadius, 0.19f * 720.0f * 0.992f, 0.06f * 0.19f * 720.0f);
}

TEST(EclipseSceneTest, NoDustInsideTheHole)
{
  SceneRun r;
  r.Frames(300, Loud(), 20);
  const float R = r.out.holeRadius / 0.992f;
  ASSERT_FALSE(r.out.dust.empty());
  for (const auto& d : r.out.dust)
    ASSERT_GE(std::hypot(d.x - r.out.holeX, d.y - r.out.holeY), R * 0.9f);
  for (const auto& d : r.out.soft)
    ASSERT_GE(std::hypot(d.x - r.out.holeX, d.y - r.out.holeY), R * 0.9f);
}

TEST(EclipseSceneTest, ElementsAtZeroDrawNothing)
{
  SceneRun r;
  r.settings.flame = r.settings.particles = r.settings.corona = r.settings.waves = 0;
  r.Frames(120, Loud(), 20);
  EXPECT_TRUE(r.out.dust.empty());
  EXPECT_TRUE(r.out.soft.empty());
  EXPECT_TRUE(r.out.under.empty());
  EXPECT_EQ(r.scene.WaveCount(), 0u);
}

TEST(EclipseSceneTest, SilenceLeavesAGlimmeringRing)
{
  SceneRun r;
  r.Frames(300, Loud(), 20);
  r.Frames(600, Features{});
  EXPECT_GT(r.scene.DustCount(), 0u);
  EXPECT_LT(r.scene.DustCount(), 4000u);
  EXPECT_LT(r.scene.CoronaCount(), 1500u);
  EXPECT_TRUE(r.out.under.empty());
  EXPECT_EQ(r.out.over.front().inner.a, 0.95f); // weißer Kernring bleibt
}

TEST(EclipseSceneTest, WaveFollowsTheRouting)
{
  SceneRun r;
  r.settings.route[kWave] = {false, false, true, false};
  r.Frames(1, Features{}, 1, {kBass});
  EXPECT_EQ(r.scene.WaveCount(), 0u);
  r.Frames(1, Features{}, 1, {kMid});
  EXPECT_EQ(r.scene.WaveCount(), 1u);
}

TEST(EclipseSceneTest, DropFiresDoubleWaveOnlyWhenEnabled)
{
  Features f;
  f.drop = true;
  SceneRun on;
  on.Frames(1, f);
  EXPECT_EQ(on.scene.WaveCount(), 2u);
  SceneRun off;
  off.settings.drop = false;
  off.Frames(1, f);
  EXPECT_EQ(off.scene.WaveCount(), 0u);
}

TEST(EclipseSceneTest, ParticleLimitHolds)
{
  SceneRun r;
  r.settings.maxParticles = 10000;
  r.settings.intensity = 100;
  r.Frames(600, Loud(), 5);
  EXPECT_LE(r.scene.DustCount(), 10000u);
  EXPECT_LE(r.scene.CoronaCount(), 10000u);
}

TEST(EclipseSceneTest, SameSeedSameResult)
{
  SceneRun a, b;
  a.Frames(200, Loud(), 15);
  b.Frames(200, Loud(), 15);
  EXPECT_EQ(a.scene.DustCount(), b.scene.DustCount());
  EXPECT_EQ(a.scene.CoronaCount(), b.scene.CoronaCount());
}

TEST(EclipseSceneTest, PixelSettingReachesDrawList)
{
  SceneRun r;
  r.settings.pixel = 6;
  r.Frames(1, Features{});
  EXPECT_FLOAT_EQ(r.out.pixel, 4.0f);
  EXPECT_FLOAT_EQ(r.out.pixelGain, std::pow(4.0f, -1.1f));
}

TEST(EclipseSceneTest, WavesStillExpireAfterDaysOfRuntime)
{
  // Prozessweite Szene nach ~7 Tagen Laufzeit: Zeit darf nicht einfrieren, Wellen müssen verfallen.
  EclipseScene scene(42, 6.0e5);
  Settings settings = Settings::Defaults();
  DrawList out;
  for (int i = 0; i < 1200; ++i) // 20 s, zwei Wellen pro Sekunde
  {
    Features f;
    if (i % 30 == 0)
      f.onset[kBass] = 0.8f;
    scene.Frame(f, settings, 1.0f / 60.0f, out);
  }
  EXPECT_LE(scene.WaveCount(), 16u);
}
