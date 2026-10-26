#include "core/SampleRing.h"

#include <gtest/gtest.h>

#include <vector>

using spektra::SampleRing;

TEST(SampleRingTest, StereoIsAveraged)
{
  SampleRing ring(8);
  ring.SetChannels(2);
  const float in[] = {1.0f, 0.0f, 0.5f, 0.5f, -1.0f, 1.0f};
  ring.Push(in, 6);
  float out[3];
  ASSERT_TRUE(ring.Latest(out, 3));
  EXPECT_FLOAT_EQ(out[0], 0.5f);
  EXPECT_FLOAT_EQ(out[1], 0.5f);
  EXPECT_FLOAT_EQ(out[2], 0.0f);
  EXPECT_EQ(ring.TotalFrames(), 3u);
}

TEST(SampleRingTest, DownmixesAnyChannelCount)
{
  for (int channels : {1, 6})
  {
    SampleRing ring(16);
    ring.SetChannels(channels);
    std::vector<float> in(4 * channels);
    for (int frame = 0; frame < 4; ++frame)
      for (int c = 0; c < channels; ++c)
        in[frame * channels + c] = static_cast<float>(frame);
    ring.Push(in.data(), in.size());
    float out[4];
    ASSERT_TRUE(ring.Latest(out, 4)) << channels;
    for (int frame = 0; frame < 4; ++frame)
      EXPECT_FLOAT_EQ(out[frame], static_cast<float>(frame)) << channels;
  }
}

TEST(SampleRingTest, PartialFramesAcrossPushesStayAligned)
{
  SampleRing ring(8);
  ring.SetChannels(2);
  const float a[] = {1.0f, 1.0f, 2.0f};
  const float b[] = {2.0f, 3.0f, 3.0f};
  ring.Push(a, 3);
  ring.Push(b, 3);
  float out[3];
  ASSERT_TRUE(ring.Latest(out, 3));
  EXPECT_FLOAT_EQ(out[0], 1.0f);
  EXPECT_FLOAT_EQ(out[1], 2.0f);
  EXPECT_FLOAT_EQ(out[2], 3.0f);
}

TEST(SampleRingTest, WrapsAroundAndKeepsNewest)
{
  SampleRing ring(4);
  ring.SetChannels(1);
  const float in[] = {1, 2, 3, 4, 5, 6};
  ring.Push(in, 6);
  float out[4];
  ASSERT_TRUE(ring.Latest(out, 4));
  EXPECT_FLOAT_EQ(out[0], 3.0f);
  EXPECT_FLOAT_EQ(out[3], 6.0f);
  EXPECT_FALSE(ring.Latest(out, 5));
}

TEST(SampleRingTest, ClearDropsOldSamples)
{
  SampleRing ring(8);
  ring.SetChannels(1);
  const float in[] = {1, 2, 3};
  ring.Push(in, 3);
  ring.Clear();
  float out[1];
  EXPECT_FALSE(ring.Latest(out, 1));
  const float more[] = {7};
  ring.Push(more, 1);
  ASSERT_TRUE(ring.Latest(out, 1));
  EXPECT_FLOAT_EQ(out[0], 7.0f);
}

TEST(SampleRingTest, NotEnoughYet)
{
  SampleRing ring(8);
  ring.SetChannels(1);
  const float in[] = {1, 2};
  ring.Push(in, 2);
  float out[3];
  EXPECT_FALSE(ring.Latest(out, 3));
}
