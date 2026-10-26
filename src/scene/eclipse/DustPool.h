#pragma once

#include "Context.h"
#include "scene/DrawList.h"
#include "scene/Random.h"

#include <cstddef>
#include <vector>

namespace spektra
{

// Ein Korn der Flamme (gebunden) oder losgelöster Staub (frei).
struct Grain
{
  float x, y, vx, vy, life, max;
  bool free;
  int size;
};

// Flammen- und freier Staub: Bewegung im Strömungsfeld, Entfernen, Zeichnen (Prototyp: px[]).
class DustPool
{
public:
  explicit DustPool(Random& rng) : m_rng(rng) {}

  size_t Size() const { return m_grains.size(); }
  const std::vector<Grain>& Grains() const { return m_grains; }
  void Add(const Grain& grain) { m_grains.push_back(grain); }

  // Staubausbruch: an `spots` Stellen reißt Staub gleichzeitig ab (Höhen-Anschlag, Drop).
  void Release(const Context& c, float strength, int spots);

  // Bewegt alle Körner, entfernt verbrauchte (und die eines Reglers auf 0) und zeichnet die übrigen.
  void Step(const Context& c, DrawList& out);

private:
  Random& m_rng;
  std::vector<Grain> m_grains;
};

} // namespace spektra
