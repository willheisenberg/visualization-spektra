#pragma once

#include "AudioAnalyzer.h"
#include "Settings.h"

namespace spektra
{

// Pegel bzw. Anschlag eines Effekts: das stärkste zugeordnete Band, ohne Zuordnung 0.
float RoutedLevel(const Settings& settings, const Features& features, int effect);
float RoutedOnset(const Settings& settings, const Features& features, int effect);

} // namespace spektra
