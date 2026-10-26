#include "Settings.h"

#include <algorithm>
#include <cstdlib>
#include <cerrno>

namespace spektra
{
namespace
{

constexpr const char* kEffectKeys[kEffectCount] = {"breath", "flame", "corona", "free",     "flow",
                                                  "wave",   "burst", "jet",    "streamer", "dust"};
constexpr const char* kBandKeys[kBandCount] = {"bass", "low", "mid", "high"};
constexpr const char* kRoutePrefix = "eclipse.route.";

struct IntSpec
{
  const char* key;
  int Settings::*field;
  int min, max;
};

constexpr IntSpec kInts[] = {
    {"intensity", &Settings::intensity, 1, 100},
    {"eclipse.flame", &Settings::flame, 0, 100},
    {"eclipse.waves", &Settings::waves, 0, 100},
    {"eclipse.particles", &Settings::particles, 0, 100},
    {"eclipse.corona", &Settings::corona, 0, 100},
    {"eclipse.pixel", &Settings::pixel, 0, 8},
    {"maxparticles", &Settings::maxParticles, 10000, 60000},
};

struct BoolSpec
{
  const char* key;
  bool Settings::*field;
};

constexpr BoolSpec kBools[] = {
    {"enabled", &Settings::enabled},
    {"autoopen", &Settings::autoopen},
    {"eclipse.drop", &Settings::drop},
};

// "eclipse.route.<effekt>.<band>" -> {effekt, band}
std::optional<std::pair<int, int>> ParseRoute(const std::string& key)
{
  if (key.rfind(kRoutePrefix, 0) != 0)
    return std::nullopt;
  const auto rest = key.substr(std::char_traits<char>::length(kRoutePrefix));
  const auto dot = rest.find('.');
  if (dot == std::string::npos)
    return std::nullopt;
  const auto effect = rest.substr(0, dot), band = rest.substr(dot + 1);
  for (int e = 0; e < kEffectCount; ++e)
    for (int b = 0; b < kBandCount; ++b)
      if (effect == kEffectKeys[e] && band == kBandKeys[b])
        return std::make_pair(e, b);
  return std::nullopt;
}

// "true"/"false" aus live.json, "1"/"0" aus Kodis SetSetting.
std::optional<bool> ParseBool(const std::string& value)
{
  if (value == "true" || value == "1")
    return true;
  if (value == "false" || value == "0")
    return false;
  return std::nullopt;
}

std::optional<long> ParseInt(const std::string& value)
{
  if (value.empty())
    return std::nullopt;
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (errno != 0 || end == nullptr || *end != '\0')
    return std::nullopt;
  return parsed;
}

} // namespace

const char* EffectKey(int effect)
{
  return kEffectKeys[effect];
}

const char* BandKey(int band)
{
  return kBandKeys[band];
}

Settings Settings::Defaults()
{
  Settings s;
  const int bands[kEffectCount] = {kBass, kLow, kMid, kHigh, kHigh, kBass, kMid, kLow, kMid, kHigh};
  for (int e = 0; e < kEffectCount; ++e)
    s.route[e][bands[e]] = true;
  return s;
}

const std::vector<std::string>& SettingKeys()
{
  static const std::vector<std::string> keys = [] {
    std::vector<std::string> k = {"enabled",           "autoopen",       "scene",         "intensity",
                                  "eclipse.flame",     "eclipse.waves",  "eclipse.particles",
                                  "eclipse.corona",    "eclipse.pixel",  "eclipse.drop"};
    for (int e = 0; e < kEffectCount; ++e)
      for (int b = 0; b < kBandCount; ++b)
        k.push_back(std::string(kRoutePrefix) + kEffectKeys[e] + "." + kBandKeys[b]);
    k.push_back("maxparticles");
    return k;
  }();
  return keys;
}

std::optional<SettingType> TypeOf(const std::string& key)
{
  if (key == "scene")
    return SettingType::String;
  for (const auto& spec : kInts)
    if (key == spec.key)
      return SettingType::Int;
  for (const auto& spec : kBools)
    if (key == spec.key)
      return SettingType::Bool;
  if (ParseRoute(key))
    return SettingType::Bool;
  return std::nullopt;
}

ApplyResult ApplySetting(Settings& settings, const std::string& key, const std::string& value)
{
  if (key == "scene")
  {
    if (value != "eclipse")
      return ApplyResult::InvalidValue;
    settings.scene = value;
    return ApplyResult::Ok;
  }
  for (const auto& spec : kInts)
    if (key == spec.key)
    {
      const auto parsed = ParseInt(value);
      if (!parsed)
        return ApplyResult::InvalidValue;
      settings.*spec.field = static_cast<int>(std::clamp<long>(*parsed, spec.min, spec.max));
      return ApplyResult::Ok;
    }
  for (const auto& spec : kBools)
    if (key == spec.key)
    {
      const auto parsed = ParseBool(value);
      if (!parsed)
        return ApplyResult::InvalidValue;
      settings.*spec.field = *parsed;
      return ApplyResult::Ok;
    }
  if (const auto route = ParseRoute(key))
  {
    const auto parsed = ParseBool(value);
    if (!parsed)
      return ApplyResult::InvalidValue;
    settings.route[route->first][route->second] = *parsed;
    return ApplyResult::Ok;
  }
  return ApplyResult::UnknownKey;
}

std::string SettingValue(const Settings& settings, const std::string& key)
{
  if (key == "scene")
    return settings.scene;
  for (const auto& spec : kInts)
    if (key == spec.key)
      return std::to_string(settings.*spec.field);
  for (const auto& spec : kBools)
    if (key == spec.key)
      return settings.*spec.field ? "true" : "false";
  if (const auto route = ParseRoute(key))
    return settings.route[route->first][route->second] ? "true" : "false";
  return "";
}

} // namespace spektra
