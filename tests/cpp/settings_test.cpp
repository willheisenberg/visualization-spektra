#include "TempDir.h"
#include "core/Settings.h"
#include "core/StateFiles.h"

#include <gtest/gtest.h>

#include <fstream>

using namespace spektra;

namespace
{
void Write(const std::string& path, const std::string& text)
{
  std::ofstream(path) << text;
}
} // namespace

TEST(SettingsTest, DefaultsMatchSpec)
{
  const auto s = Settings::Defaults();
  EXPECT_TRUE(s.enabled);
  EXPECT_TRUE(s.autoopen);
  EXPECT_EQ(s.scene, "eclipse");
  EXPECT_EQ(s.intensity, 70);
  EXPECT_EQ(s.flame, 100);
  EXPECT_EQ(s.waves, 100);
  EXPECT_EQ(s.particles, 100);
  EXPECT_EQ(s.corona, 100);
  EXPECT_EQ(s.pixel, 2);
  EXPECT_FLOAT_EQ(kPixelSteps[s.pixel], 1.0f);
  EXPECT_TRUE(s.drop);
  EXPECT_EQ(s.maxParticles, 42000);
  const int expected[kEffectCount] = {kBass, kLow, kMid, kHigh, kHigh, kBass, kMid, kLow, kMid, kHigh};
  for (int e = 0; e < kEffectCount; ++e)
    for (int b = 0; b < kBandCount; ++b)
      EXPECT_EQ(s.route[e][b], b == expected[e]) << EffectKey(e) << "/" << BandKey(b);
}

TEST(SettingsTest, KeysAreCompleteAndTyped)
{
  EXPECT_EQ(SettingKeys().size(), 10u + 40u + 1u);
  EXPECT_EQ(TypeOf("intensity"), SettingType::Int);
  EXPECT_EQ(TypeOf("eclipse.route.wave.bass"), SettingType::Bool);
  EXPECT_EQ(TypeOf("scene"), SettingType::String);
  EXPECT_FALSE(TypeOf("eclipse.route.wave.treble").has_value());
}

TEST(SettingsTest, ApplyParsesAndClamps)
{
  auto s = Settings::Defaults();
  EXPECT_EQ(ApplySetting(s, "intensity", "40"), ApplyResult::Ok);
  EXPECT_EQ(s.intensity, 40);
  EXPECT_EQ(ApplySetting(s, "intensity", "0"), ApplyResult::Ok);
  EXPECT_EQ(s.intensity, 1);
  EXPECT_EQ(ApplySetting(s, "eclipse.flame", "250"), ApplyResult::Ok);
  EXPECT_EQ(s.flame, 100);
  EXPECT_EQ(ApplySetting(s, "eclipse.pixel", "8"), ApplyResult::Ok);
  EXPECT_EQ(s.pixel, 8);
  EXPECT_EQ(ApplySetting(s, "eclipse.route.wave.high", "true"), ApplyResult::Ok);
  EXPECT_TRUE(s.route[kWave][kHigh]);
  EXPECT_EQ(ApplySetting(s, "maxparticles", "5"), ApplyResult::Ok);
  EXPECT_EQ(s.maxParticles, 10000);
}

TEST(SettingsTest, KodiBoolsArriveAsDigits)
{
  // Kodi 21 übergibt Bools an SetSetting als "1"/"0" (CSettingValue).
  auto s = Settings::Defaults();
  EXPECT_EQ(ApplySetting(s, "eclipse.drop", "0"), ApplyResult::Ok);
  EXPECT_FALSE(s.drop);
  EXPECT_EQ(ApplySetting(s, "eclipse.route.wave.high", "1"), ApplyResult::Ok);
  EXPECT_TRUE(s.route[kWave][kHigh]);
}

TEST(SettingsTest, ApplyRejectsGarbage)
{
  auto s = Settings::Defaults();
  EXPECT_EQ(ApplySetting(s, "intensity", "viel"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "intensity", "40x"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "eclipse.drop", "ja"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "scene", "matrix"), ApplyResult::InvalidValue);
  EXPECT_EQ(ApplySetting(s, "farbe", "rot"), ApplyResult::UnknownKey);
  EXPECT_EQ(s.intensity, 70);
}

TEST(SettingsTest, ValueRoundTrips)
{
  auto s = Settings::Defaults();
  for (const auto& key : SettingKeys())
  {
    auto copy = Settings::Defaults();
    EXPECT_EQ(ApplySetting(copy, key, SettingValue(s, key)), ApplyResult::Ok) << key;
    EXPECT_EQ(SettingValue(copy, key), SettingValue(s, key)) << key;
  }
}

TEST(SettingsTest, LiveJsonIsApplied)
{
  TempDir dir;
  Write(dir.File("live.json"),
        R"({"revision": 7, "settings": {"intensity": 25, "eclipse.drop": false, "eclipse.route.burst.bass": true,
            "scene": "eclipse", "unbekannt": 3}})");
  const auto live = ReadLiveSettings(dir.File("live.json"), Settings::Defaults());
  ASSERT_TRUE(live.has_value());
  EXPECT_EQ(live->revision, 7);
  EXPECT_EQ(live->settings.intensity, 25);
  EXPECT_FALSE(live->settings.drop);
  EXPECT_TRUE(live->settings.route[kBurst][kBass]);
  EXPECT_TRUE(live->settings.route[kBurst][kMid]);
}

TEST(SettingsTest, LiveJsonGarbageIsIgnored)
{
  TempDir dir;
  auto base = Settings::Defaults();
  base.intensity = 33;
  EXPECT_FALSE(ReadLiveSettings(dir.File("fehlt.json"), base).has_value());
  Write(dir.File("a.json"), "kein json");
  EXPECT_FALSE(ReadLiveSettings(dir.File("a.json"), base).has_value());
  Write(dir.File("b.json"), R"({"settings": {"intensity": 10}})");
  EXPECT_FALSE(ReadLiveSettings(dir.File("b.json"), base).has_value());
  Write(dir.File("c.json"), R"({"revision": 2, "settings": {"intensity": "zehn", "eclipse.flame": [1]}})");
  const auto live = ReadLiveSettings(dir.File("c.json"), base);
  ASSERT_TRUE(live.has_value());
  EXPECT_EQ(live->settings.intensity, 33);
  EXPECT_EQ(live->settings.flame, 100);
}

#include <regex>
#include <sstream>

TEST(SettingsTest, SettingsXmlMatchesDefaults)
{
  std::ifstream in(std::string(SPEKTRA_REPO_ROOT) + "/visualization.spektra/resources/settings.xml");
  ASSERT_TRUE(in);
  std::stringstream text;
  text << in.rdbuf();
  const std::string xml = text.str();
  const std::regex re(R"re(<setting id="([^"]+)"[^>]*default="([^"]+)")re");
  const auto defaults = Settings::Defaults();
  size_t seen = 0;
  for (auto it = std::sregex_iterator(xml.begin(), xml.end(), re); it != std::sregex_iterator(); ++it, ++seen)
    EXPECT_EQ((*it)[2].str(), SettingValue(defaults, (*it)[1].str())) << (*it)[1].str();
  EXPECT_EQ(seen, SettingKeys().size());
}
