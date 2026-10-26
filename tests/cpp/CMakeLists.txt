cmake_minimum_required(VERSION 3.18)
project(spektra_core_tests CXX)

# Eigenständiger amd64-Testbuild der Kodi- und GL-freien Einheiten aus src/core und src/scene.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Werror)

set(CORE_SOURCES
  ${REPO_ROOT}/src/core/Fft.cpp
  ${REPO_ROOT}/src/core/Spectrum.cpp
  ${REPO_ROOT}/src/core/AudioAnalyzer.cpp
  ${REPO_ROOT}/src/core/Settings.cpp
  ${REPO_ROOT}/src/core/Routing.cpp
  ${REPO_ROOT}/src/core/StateFiles.cpp
  ${REPO_ROOT}/src/core/SampleRing.cpp
  ${REPO_ROOT}/src/core/Engine.cpp
  ${REPO_ROOT}/src/scene/eclipse/DustPool.cpp
  ${REPO_ROOT}/src/scene/eclipse/Flame.cpp
  ${REPO_ROOT}/src/scene/eclipse/Corona.cpp
  ${REPO_ROOT}/src/scene/eclipse/EclipseScene.cpp
)

add_library(spektra_core STATIC ${CORE_SOURCES})
target_include_directories(spektra_core PUBLIC ${REPO_ROOT}/src)
target_include_directories(spektra_core SYSTEM PUBLIC ${REPO_ROOT}/third_party)
target_link_libraries(spektra_core PUBLIC Threads::Threads)

set(TEST_SOURCES
  fft_test.cpp
  spectrum_test.cpp
  audio_analyzer_test.cpp
  settings_test.cpp
  routing_test.cpp
  state_files_test.cpp
  sample_ring_test.cpp
  engine_test.cpp
  scene_basics_test.cpp
  flame_dust_test.cpp
  corona_test.cpp
  eclipse_scene_test.cpp
)

add_executable(core_tests ${TEST_SOURCES})
target_link_libraries(core_tests PRIVATE spektra_core GTest::gtest_main)
target_compile_definitions(core_tests PRIVATE SPEKTRA_REPO_ROOT="${REPO_ROOT}")

enable_testing()
include(GoogleTest)
gtest_discover_tests(core_tests DISCOVERY_TIMEOUT 30)
