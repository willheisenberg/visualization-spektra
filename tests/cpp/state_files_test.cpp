#include "TempDir.h"
#include "core/StateFiles.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace spektra;

TEST(StateFilesTest, RendererStatusIsWrittenAtomically)
{
  TempDir dir;
  const auto path = dir.File("sub/renderer.json");
  RendererStatus status;
  status.fps = 58.5f;
  ASSERT_TRUE(WriteRendererStatus(path, status));
  EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
  std::ifstream in(path);
  const auto json = nlohmann::json::parse(in);
  EXPECT_EQ(json["state"], "running");
  EXPECT_EQ(json["scene"], "eclipse");
  EXPECT_NEAR(json["fps"].get<double>(), 58.5, 1e-3);
  EXPECT_EQ(json["error"], "");
  EXPECT_GT(json["time"].get<int64_t>(), 1700000000);
}
