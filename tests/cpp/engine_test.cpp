#include "TempDir.h"
#include "core/Engine.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace spektra;

namespace
{
struct Harness
{
  TempDir dir;
  double now = 0.0;
  Engine engine{EngineConfig{dir.Path().string(), [this] { return now; }}};
  double phase = 0.0;

  // Ein Bild bei 60 fps; optional 735 Stereo-Frames Sinus der Frequenz hz mit Amplitude amp.
  const Features& Frame(double hz, float amp = 0.5f)
  {
    now += 1.0 / 60.0;
    if (hz > 0.0)
    {
      std::vector<float> samples(735 * 2);
      for (size_t i = 0; i < 735; ++i)
      {
        const float v = amp * static_cast<float>(std::sin(phase));
        phase += 2.0 * M_PI * hz / 44100.0;
        samples[2 * i] = samples[2 * i + 1] = v;
      }
      engine.PushAudio(samples.data(), samples.size());
    }
    return engine.Tick();
  }
  void Run(double hz, double seconds, float amp = 0.5f)
  {
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i)
      Frame(hz, amp);
  }
};
} // namespace

TEST(EngineTest, ToneDrivesItsBand)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  const auto& f = h.engine.Tick();
  EXPECT_GT(f.level[kMid], 0.2f);
  EXPECT_LT(f.level[kBass], 0.05f);
}

TEST(EngineTest, SilenceAfterMusicDecays)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  h.Run(0.0, 3.0);
  for (float l : h.engine.Tick().level)
    EXPECT_LT(l, 0.05f);
}

TEST(EngineTest, StartKeepsAnalysisState)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  const float before = h.engine.Tick().level[kMid];
  ASSERT_GT(before, 0.2f);
  h.engine.Start(2, 44100); // Songwechsel: neue Kodi-Instanz
  EXPECT_NEAR(h.Frame(1000.0).level[kMid], before, 0.05f);
}

TEST(EngineTest, GapTriggersRebase)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Run(1000.0, 3.0);
  h.now += 5.0; // Spulen: 5 s keine Bilder und kein Audio
  h.Frame(0.0);
  // Danach deutlich leiser: ohne Neueinpegeln bliebe der Pegel klein (≈ 0,1), mit ≈ 0,4.
  h.Run(1000.0, 2.0, 0.05f);
  EXPECT_GT(h.engine.Tick().level[kMid], 0.2f);
}

TEST(EngineTest, LiveJsonIsPickedUpOncePerRevision)
{
  Harness h;
  h.engine.Start(2, 44100);
  std::ofstream(h.dir.File("live.json")) << R"({"revision": 3, "settings": {"intensity": 10}})";
  h.Run(0.0, 1.5);
  EXPECT_EQ(h.engine.CurrentSettings().intensity, 10);
  h.engine.ApplyKodiSetting("intensity", "50");
  h.Run(0.0, 1.5); // gleiche Revision: Kodi-Wert bleibt
  EXPECT_EQ(h.engine.CurrentSettings().intensity, 50);
}

TEST(EngineTest, HeartbeatIsRateLimited)
{
  Harness h;
  const auto path = h.dir.File("renderer.json");
  h.engine.Heartbeat("running", 60.0f, "");
  ASSERT_TRUE(std::filesystem::exists(path));
  std::filesystem::remove(path);
  h.now += 0.5;
  h.engine.Heartbeat("running", 60.0f, "");
  EXPECT_FALSE(std::filesystem::exists(path));
  h.now += 0.6;
  h.engine.Heartbeat("error", 0.0f, "gl_init");
  std::ifstream in(path);
  const auto json = nlohmann::json::parse(in);
  EXPECT_EQ(json["state"], "error");
  EXPECT_EQ(json["error"], "gl_init");
}

TEST(EngineTest, DtIsClampedForSimulation)
{
  Harness h;
  h.engine.Start(2, 44100);
  h.Frame(0.0);
  h.now += 2.0;
  h.engine.Tick();
  EXPECT_FLOAT_EQ(h.engine.LastDt(), 0.1f);
}
