FROM debian:bookworm

ARG KODI_TAG=21.3-Omega

RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential crossbuild-essential-arm64 cmake git ca-certificates \
      pkg-config zip unzip file \
      libgles-dev:arm64 libgtest-dev \
 && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch "${KODI_TAG}" https://github.com/xbmc/xbmc.git /opt/kodi

COPY aarch64-toolchain.cmake /opt/toolchains/aarch64.cmake
