# SIGPROC

Hypermodern C++23 rewrite of SIGPROC — filterbank-native pulsar/FRB
processing. Library plus `sig_*` command-line tools.

Based on Evan Keane's [fork](https://github.com/FRBs/sigproc) of Michael Keith's
[release](https://github.com/SixByNine/sigproc) of Duncan Lorimer's original
[SIGPROC](http://sigproc.sourceforge.net/). Library layout takes inspiration from
[sigpyproc3](https://github.com/FRBs/sigpyproc3).

[![GitHub CI](https://github.com/pravirkr/sigproc2/actions/workflows/build.yml/badge.svg)](https://github.com/pravirkr/sigproc2/actions/workflows/build.yml)

See [AGENTS.md](AGENTS.md) for project direction, style, and the executable
parity inventory. The original SIGPROC manual is [`docs/sigproc.pdf`](docs/sigproc.pdf).
The staffable rewrite plan is [`docs/implementation-plan.md`](docs/implementation-plan.md);
the later search stack (not staffable yet) is [`docs/future-plan.md`](docs/future-plan.md).

## Requirements

- GCC >= 14.2 or LLVM Clang >= 18.0
- CMake >= 3.28 and Ninja
- OpenMP, single-precision FFTW, HDF5

CPM fetches fmt, spdlog, CLI11, HighFive, xsimd, and Catch2. Set
`CPM_SOURCE_CACHE` to cache downloads.

Release builds use `-O3 -ffast-math`. `-march=native` is on by default
(`-DSIG_ENABLE_NATIVE_ARCH=OFF` to disable).

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Tests:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIG_BUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Installed tools (names are `sig_*` so they do not clash with original SIGPROC):
`sig_header`, `sig_bandpass`, `sig_decimate`, `sig_chopfil`.
