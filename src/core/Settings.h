#pragma once

#include "AudioAnalyzer.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace spektra
{

enum Effect : int
{
  kBreath,
  kFlame,
  kCorona,
  kFree,
  kFlow,
  kWave,
  kBurst,
  kJet,
  kStreamer,
  kDust
};
constexpr int kEffectCount = 10;
constexpr float kPixelSteps[9] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};

const char* EffectKey(int effect);
const char* BandKey(int band);

struct Settings
{
  bool enabled = true;
  bool autoopen = true;
  std::string scene = "eclipse";
  int intensity = 70;
  int flame = 100;
  int waves = 100;
  int particles = 100;
  int corona = 100;
  int pixel = 2;
  bool drop = true;
  std::array<std::array<bool, kBandCount>, kEffectCount> route{};
  int maxParticles = 42000;

  static Settings Defaults();
};

enum class SettingType
{
  Bool,
  Int,
  String
};

enum class ApplyResult
{
  Ok,
  UnknownKey,
  InvalidValue
};

const std::vector<std::string>& SettingKeys();
std::optional<SettingType> TypeOf(const std::string& key);
ApplyResult ApplySetting(Settings& settings, const std::string& key, const std::string& value);
std::string SettingValue(const Settings& settings, const std::string& key);

} // namespace spektra
