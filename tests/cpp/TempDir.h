#pragma once

#include <filesystem>
#include <random>
#include <string>

// Temporäres Verzeichnis für einen Test; wird im Destruktor entfernt.
class TempDir
{
public:
  TempDir()
  {
    std::random_device rd;
    m_path = std::filesystem::temp_directory_path() / ("spektra-test-" + std::to_string(rd()));
    std::filesystem::create_directories(m_path);
  }
  ~TempDir() { std::filesystem::remove_all(m_path); }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& Path() const { return m_path; }
  std::string File(const std::string& name) const { return (m_path / name).string(); }

private:
  std::filesystem::path m_path;
};
