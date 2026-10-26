#include "core/Routing.h"

#include <gtest/gtest.h>

using namespace spektra;

TEST(RoutingTest, DefaultRouteTakesAssignedBand)
{
  const auto s = Settings::Defaults();
  Features f;
  f.level = {0.1f, 0.2f, 0.3f, 0.4f};
  f.onset = {0.5f, 0.0f, 0.7f, 0.0f};
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kFlame), 0.2f);
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kCorona), 0.3f);
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kWave), 0.5f);
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kBurst), 0.7f);
}

TEST(RoutingTest, StrongestOfSeveralBandsWins)
{
  auto s = Settings::Defaults();
  s.route[kWave][kHigh] = true;
  Features f;
  f.onset = {0.2f, 0.0f, 0.0f, 0.9f};
  EXPECT_FLOAT_EQ(RoutedOnset(s, f, kWave), 0.9f);
}

TEST(RoutingTest, NoBandMeansOff)
{
  auto s = Settings::Defaults();
  s.route[kFlame] = {false, false, false, false};
  Features f;
  f.level = {1.0f, 1.0f, 1.0f, 1.0f};
  EXPECT_FLOAT_EQ(RoutedLevel(s, f, kFlame), 0.0f);
}
