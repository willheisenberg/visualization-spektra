#pragma once

#include "Settings.h"

#include <cstdint>
#include <optional>
#include <string>

namespace spektra
{

// live.json (vom Service geschrieben, Spec §7.1).
struct LiveSettings
{
  int64_t revision = 0;
  Settings settings;
};

// std::nullopt, wenn die Datei fehlt, kein Objekt ist oder keine ganzzahlige "revision" hat.
// Einträge in "settings" werden auf base angewendet; Unbekanntes und Ungültiges wird übersprungen.
std::optional<LiveSettings> ReadLiveSettings(const std::string& path, const Settings& base);

// renderer.json (Herzschlag für die Recovery im Service, Spec §8.1).
struct RendererStatus
{
  std::string state = "running"; // running | error
  std::string scene = "eclipse";
  float fps = 0.0f;
  std::string error;
};

// Schreibt atomar (path + ".tmp", dann rename) und legt das Verzeichnis bei Bedarf an.
bool WriteRendererStatus(const std::string& path, const RendererStatus& status);

} // namespace spektra
