#!/usr/bin/env bash
#
# Cloud Agent bootstrap for sigproc2.
#
# sigproc2 is a C++23 HPC project targeting GCC >= 14.2 or Clang >= 18,
# and CMake >= 3.28 with Ninja. System dependencies include OpenMP, single-precision
# FFTW, and HDF5.
#
# Designed for Ubuntu 24.04 LTS (Noble Numbat).
# Idempotent: re-running is a fast no-op once tools are installed.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

# Ensure 'universe' is enabled (supplies gcc-14 and clang-18 natively on Ubuntu 24.04)
if ! grep -Eq "^deb .* noble(-updates)? universe" /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null; then
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends software-properties-common
  sudo add-apt-repository -y universe
fi

# Install compilers, build tools, and core HPC numerical libraries
sudo apt-get install -y --no-install-recommends \
  gcc-14 g++-14 \
  clang-18 clang++-18 lld-18 \
  cmake ninja-build \
  libfftw3-dev libhdf5-dev libomp-dev \
  git curl ca-certificates

# Set GCC 14 as the primary system compiler alternatives
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-14 \
  --slave /usr/bin/gcov gcov /usr/bin/gcov-14
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-14 140
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 140
sudo update-alternatives --set cc /usr/bin/gcc-14
sudo update-alternatives --set c++ /usr/bin/g++-14

# Register Clang 18 alternatives for easy switching
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
  --slave /usr/bin/clang++ clang++ /usr/bin/clang++-18

# Configure CPM package cache and CMake project
export CC=/usr/bin/gcc-14
export CXX=/usr/bin/g++-14
export CPM_SOURCE_CACHE="${CPM_SOURCE_CACHE:-$HOME/.cache/CPM}"
mkdir -p "$CPM_SOURCE_CACHE"

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIG_BUILD_TESTING=ON

echo "=== sigproc2 build environment ready ==="
echo "Compiler: $(gcc --version | head -1)"
echo "Clang:    $(clang-18 --version | head -1)"
echo "CMake:    $(cmake --version | head -1)"
echo "Ninja:    ninja $(ninja --version)"
echo "Build:    cmake --build build --parallel \"\$(nproc)\""
