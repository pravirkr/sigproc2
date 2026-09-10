#!/usr/bin/env bash
#
# Cloud Agent bootstrap for sigproc2.
#
# sigproc2 is a C++23 project that needs a newer toolchain than Ubuntu 24.04
# ships (GCC >= 15 and CMake >= 3.30), plus single-precision FFTW (with OpenMP)
# and Ninja. This script provisions those and then configures the project,
# which downloads and caches the CPM packages (spdlog, scnlib, CLI11, Catch2).
#
# It is idempotent: re-running it is a fast no-op once the tools are present.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

# GCC 15 via the Ubuntu toolchain test PPA (base image ships GCC 13).
if ! command -v g++-15 >/dev/null 2>&1; then
  sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
fi

# CMake >= 3.30 from Kitware's APT repository (base image ships 3.28).
if [ ! -f /usr/share/keyrings/kitware-archive-keyring.gpg ]; then
  curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | sudo gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg
  echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
    | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
fi

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  gcc-15 g++-15 cmake ninja-build libfftw3-dev git curl ca-certificates

# Make GCC 15 the default C/C++ compiler (base image resolves cc/c++ to clang).
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-15 150 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-15
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-15 150
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-15 150
sudo update-alternatives --set cc /usr/bin/gcc-15
sudo update-alternatives --set c++ /usr/bin/g++-15

# Configure the project; resolves/caches CPM dependencies and writes the Ninja
# build files into ./build. Build the targets with: cmake --build build
export CPM_SOURCE_CACHE="${CPM_SOURCE_CACHE:-$HOME/.cache/CPM}"
mkdir -p "$CPM_SOURCE_CACHE"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "sigproc2 env ready: $(gcc --version | head -1) | $(cmake --version | head -1) | ninja $(ninja --version)"
