cmake_minimum_required(VERSION 3.18)
project(visualization.spektra CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Kodi REQUIRED)

find_path(GLES3_INCLUDE_DIR GLES3/gl3.h REQUIRED)
find_library(GLESV2_LIBRARY GLESv2 REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/..
                    ${GLES3_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/third_party)

set(SPEKTRA_SOURCES src/addon.cpp
                    src/core/AudioAnalyzer.cpp
                    src/core/Engine.cpp
                    src/core/Fft.cpp
                    src/core/Routing.cpp
                    src/core/SampleRing.cpp
                    src/core/Settings.cpp
                    src/core/Spectrum.cpp
                    src/core/StateFiles.cpp
                    src/gl/Renderer.cpp
                    src/scene/eclipse/Corona.cpp
                    src/scene/eclipse/DustPool.cpp
                    src/scene/eclipse/EclipseScene.cpp
                    src/scene/eclipse/Flame.cpp)

set(SPEKTRA_HEADERS src/addon.h
                    src/core/AudioAnalyzer.h
                    src/core/Engine.h
                    src/core/Fft.h
                    src/core/Routing.h
                    src/core/SampleRing.h
                    src/core/Settings.h
                    src/core/Spectrum.h
                    src/core/StateFiles.h
                    src/gl/Renderer.h
                    src/scene/DrawList.h
                    src/scene/PixelGrid.h
                    src/scene/Random.h
                    src/scene/Scene.h
                    src/scene/eclipse/Context.h
                    src/scene/eclipse/Corona.h
                    src/scene/eclipse/DustPool.h
                    src/scene/eclipse/EclipseScene.h
                    src/scene/eclipse/Flame.h
                    src/scene/eclipse/Palette.h)

set(DEPLIBS ${GLESV2_LIBRARY})

build_addon(visualization.spektra SPEKTRA DEPLIBS)

# Kodi ruft beim Schließen der Visualisierung dlclose; die prozessweite Engine soll das überleben.
target_link_options(visualization.spektra PRIVATE -Wl,-z,nodelete)

include(CPack)
