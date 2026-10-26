#include "StateFiles.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace spektra
{

std::optional<LiveSettings> ReadLiveSettings(const std::string& path, const Settings& base)
{
  std::ifstream in(path);
  if (!in)
    return std::nullopt;
  const auto json = nlohmann::json::parse(in, nullptr, false);
  if (json.is_discarded() || !json.is_object())
    return std::nullopt;
  const auto revision = json.find("revision");
  if (revision == json.end() || !revision->is_number_integer())
    return std::nullopt;
  LiveSettings live;
  live.revision = revision->get<int64_t>();
  live.settings = base;
  const auto settings = json.find("settings");
  if (settings != json.end() && settings->is_object())
    for (const auto& [key, value] : settings->items())
    {
      std::string text;
      if (value.is_boolean())
        text = value.get<bool>() ? "true" : "false";
      else if (value.is_number_integer())
        text = std::to_string(value.get<int64_t>());
      else if (value.is_string())
        text = value.get<std::string>();
      else
        continue;
      ApplySetting(live.settings, key, text);
    }
  return live;
}

bool WriteRendererStatus(const std::string& path, const RendererStatus& status)
{
  std::error_code ec;
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty())
    std::filesystem::create_directories(parent, ec);
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  const nlohmann::json json = {
      {"state", status.state}, {"scene", status.scene}, {"fps", status.fps}, {"error", status.error}, {"time", now}};
  const auto temporary = path + ".tmp";
  {
    std::ofstream out(temporary, std::ios::trunc);
    if (!out)
      return false;
    out << json.dump();
    if (!out.flush())
      return false;
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    std::remove(temporary.c_str());
    return false;
  }
  return true;
}

} // namespace spektra
