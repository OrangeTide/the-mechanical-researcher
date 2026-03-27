#!/bin/sh
# fetch-sdl3.sh -- download and build SDL3 for the Triton emulator
#
# Fetches a specific SDL3 release, builds it as a static library, and
# installs into sdl3/ in the current directory. The Makefile picks this
# up automatically when building the SDL3-enabled emulator.
#
# Prerequisites: cmake, a C compiler, and platform video libraries.
#
# Linux (Debian/Ubuntu):
#   apt install cmake build-essential libx11-dev libxext-dev \
#       libwayland-dev libxkbcommon-dev libdrm-dev libgbm-dev \
#       libasound2-dev libpulse-dev libdbus-1-dev libudev-dev
#
# macOS:
#   brew install cmake
#
# Usage:
#   sh fetch-sdl3.sh

set -e

SDL_VERSION=3.4.2
SDL_DIR="SDL-release-${SDL_VERSION}"
SDL_TAR="${SDL_DIR}.tar.gz"
SDL_URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-${SDL_VERSION}.tar.gz"
PREFIX="$(pwd)/sdl3"

if [ -f "${PREFIX}/lib/libSDL3.a" ] || [ -f "${PREFIX}/lib64/libSDL3.a" ]; then
    echo "SDL3 already built in ${PREFIX}"
    exit 0
fi

echo "Fetching SDL ${SDL_VERSION}..."
if [ ! -f "${SDL_TAR}" ]; then
    curl -L -o "${SDL_TAR}" "${SDL_URL}"
fi

echo "Extracting..."
tar xf "${SDL_TAR}"

echo "Building SDL3 (static library)..."
cmake -S "${SDL_DIR}" -B "${SDL_DIR}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DSDL_SHARED=OFF \
    -DSDL_STATIC=ON \
    -DSDL_TEST=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_X11_XTEST=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

cmake --build "${SDL_DIR}/build" --parallel
cmake --install "${SDL_DIR}/build"

echo "Cleaning up source..."
rm -rf "${SDL_DIR}" "${SDL_TAR}"

echo ""
echo "SDL3 ${SDL_VERSION} installed to ${PREFIX}"
echo "Run 'make triton' to build the emulator with SDL3 display."
