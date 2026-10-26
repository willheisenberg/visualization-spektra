#include "Routing.h"

#include <algorithm>

namespace spektra
{

float RoutedLevel(const Settings& settings, const Features& features, int effect)
{
  float v = 0.0f;
  for (int b = 0; b < kBandCount; ++b)
    if (settings.route[effect][b])
      v = std::max(v, features.level[b]);
  return v;
}

float RoutedOnset(const Settings& settings, const Features& features, int effect)
{
  float v = 0.0f;
  for (int b = 0; b < kBandCount; ++b)
    if (settings.route[effect][b])
      v = std::max(v, features.onset[b]);
  return v;
}

} // namespace spektra
