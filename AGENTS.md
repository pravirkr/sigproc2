# Agent Guidelines for sigproc2

Rules for AI agents (and contributors). Do not renegotiate style, dependencies,
or project goals in chat. Follow this file and the checked-in style configs.

## What this repo is

A **C++23 HPC rewrite** of SIGPROC: a library (`libsigproc`) plus command-line
tools in `applications/`. It is not a line-by-line port of the original C/Fortran
tree.

This iteration is **strictly C++**. Python bindings are out of scope. Do not add
pybind11, nanobind, SWIG, or a Python package layout.

## Sources of truth

Use these in this order. Do not invent a different architecture or tool set.

1. **Executable behavior.** Original SIGPROC
   ([FRBs/sigproc](https://github.com/FRBs/sigproc)) and
   [`docs/sigproc.pdf`](docs/sigproc.pdf). A rewritten tool must match original
   capability or add to it. No silent regressions in numerics, SIGPROC header
   keys, or the filterbank on-disk format. CLI flags may be modernized with
   CLI11 if they remain a **superset** of the original.
2. **Library shape.** The current `include/sigproc/` public API. Rearrange
   internally in modern C++, but do not start a new layout. Seek inspiration from
   [FRBs/sigpyproc3](https://github.com/FRBs/sigpyproc3) for design (`Header`, `FilReader`,
   `block` / `timeseries` / `fourierseries`, `core/kernels`, `io/bits`). Do not
   copy Python APIs blindly.
3. **Style.** [`.clang-format`](.clang-format), [`.clang-tidy`](.clang-tidy),
   [`.cmake-format.yaml`](.cmake-format.yaml). Run `clang-format` before
   committing. Do not re-litigate these files.

## Scope

**In scope:** filterbank-native processing — header/IO, bit pack/unpack, kernels,
then fake, splice, extract, downsample, flatten, zerodm, clip/blanker, reader,
filedit, dice, dedisperse, fold, seek, RFI.

**Out of scope:** Python bindings; historical raw-backend converters (`wapp2fb`,
`bpp2fb`, `scamp2fb`, `pspm2fb`, …); PGPLOT UIs. PSRFITS is later, not now.

**Binary names** use a `sig_` prefix (no PATH clash with original SIGPROC):

| This rewrite    | Original SIGPROC |
|-----------------|------------------|
| `sig_header`    | `header`         |
| `sig_bandpass`   | `bandpass`       |
| `sig_decimate`   | `decimate`       |
| `sig_chopfil`    | `chop_fil`       |

Map new tools the same way: `sig_<original_name>` with underscores, not hyphens.

## Two-layer C++ (HPC)

This is high-performance scientific software. Never sacrifice hot-path
performance for fashion.

**Public API** (`include/sigproc/`): modern C++23. Prefer `std::span`, concepts,
`std::format` / `std::print`, `std::optional`, `std::string_view`, RAII,
exceptions (`std::runtime_error`, `std::invalid_argument`). Document public
classes and functions with Doxygen comments.

**Inner kernels** (`lib/`, `sigproc::bits`, `sigproc::kernels`): as low-level as
needed. Raw loops, pointer arithmetic, lookup tables, OpenMP, and
auto-vectorization are expected. Use "__restrict__" for pointers where appropriate.
Do not wrap hot loops in `std::for_each`, `std::any`, or extra heap allocations
for style. Do not introduce owning `new`/`delete`; use `std::vector` / `std::array`
for storage and `std::span` at the API boundary.

- Parallelism: **OpenMP only**. No `std::execution`, TBB, or other runtimes.
- Prefer auto-vectorization. **xsimd** is available for measured hot paths; do
  not sprinkle SIMD through the public API.
- Release builds use `-O3 -ffast-math` by default. `-march=native` is
  switchable (`-DSIG_ENABLE_NATIVE_ARCH=ON`, default ON).

## Headers and includes

`include/sigproc/` is the **public, installable API**. Applications, tests, and
downstream users include these with **angle brackets**: `<sigproc/...>`.

`lib/sigproc/` holds **private, non-installable** headers. They are visible only
to the library via a PRIVATE include directory (`lib/`). Include them with
**quotes**: `"sigproc/..."`. Never put private headers under `include/`. Never
include private headers from public headers, applications, or tests.

Include order (enforced by `.clang-format` `IncludeCategories`):

1. Definition file (the `.cpp`'s matching header; always first)
2. System headers (`<cmath>`, `<vector>`, …)
3. Third-party headers (`<CLI/...>`, `<fmt/...>`, `<spdlog/...>`, `<omp.h>`, …)
4. sigproc public headers (`<sigproc/...>`)
5. sigproc private headers (`"sigproc/..."`)

Public headers include only system, third-party, and other public sigproc
headers. They must stay self-contained without private helpers.

## Naming

Follow `.clang-tidy` `readability-identifier-naming`:

- Classes, structs, enums, type aliases: `CamelCase`
- Functions, variables, parameters, members, namespaces: `lower_case`
- Private/protected members: `m_` prefix
- Constants / `constexpr`: `CamelCase` with `k` prefix (`kDispConst`)
- Macros: `UPPER_CASE`

## Dependencies

Managed in [`cmake/sigprocDependencies.cmake`](cmake/sigprocDependencies.cmake).
Do not add Boost or extra third-party libraries without an explicit maintainer
request. Logging is **spdlog**. Project code uses `std::format`; fmt is pinned as spdlog's backend.
fmt formatting can be used where `std::format` is not adequate.

**Required to build**

- GCC >= 14.2 or LLVM Clang >= 18.0 (AppleClang is not supported)
- CMake >= 3.28, Ninja
- OpenMP, single-precision FFTW (`fftw3f`), HDF5

**CPM (fetched, pinned)**

- fmt 12.1, spdlog 1.17 (header-only, spdlog uses external fmt)
- CLI11 2.7 — applications
- HighFive 3.3, xsimd 14 — build-tree only
- Catch2 3.4 — tests only (`-DSIG_BUILD_TESTING=ON`)

Set `CPM_SOURCE_CACHE` to cache downloads. Cloud Agent setup:
[`.cursor/environment.json`](.cursor/environment.json) and
[`.cursor/install.sh`](.cursor/install.sh). Keep them in sync with this list.

Do not put spdlog, fmt, HighFive, or xsimd includes in public headers.

CMake sources use `file(GLOB … CONFIGURE_DEPENDS)`. Do not replace GLOB with
an explicit `.cpp` list.

## Build, test, sanitizers, coverage

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIG_BUILD_TESTING=ON
cmake --build build && ctest --test-dir build --output-on-failure

cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSIG_BUILD_TESTING=ON

cmake -S . -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DSIG_BUILD_TESTING=ON -DSIG_ENABLE_COVERAGE=ON

# Portable Release (no -march=native); -ffast-math stays on
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIG_ENABLE_NATIVE_ARCH=OFF
```

Install installs the library, public headers, CMake package config, and `sig_*`
executables. `lib/sigproc/` is not installed.

## Git workflow

- **Never commit to `main`.** Work on a branch and open a Pull Request. The
  maintainer reviews and merges. Do not merge PRs yourself.
- Commit and PR messages: concise, describe all the changes, no filler.
- One logical change per commit; do not force-push or amend unless asked.
- **When in doubt, ask.** If requirements or scope are unclear, ask the
  maintainer rather than guessing.

## Parity inventory

**Done**

- `sig_header` ← `header`
- `sig_bandpass` ← `bandpass`
- `sig_decimate` ← `decimate`
- `sig_chopfil` ← `chop_fil`
- `sig_fake` ← `fake` (library `sigproc::fake`; smear `8.3e3`, delay `4148.741601`)
- `sig_fast_fake` ← `fast_fake` (trampoline)
- `sig_extract` ← `extract`
- `sig_downsample` ← `downsample`
- `sig_splice` ← `splice` (always `FREQUENCY_START` table)
- `sig_dice` ← `dice` (1-based keep file; default force-zeros)
- `sig_flatten` ← `flatten` (`TimeSeries`, gulp-median, `data_type=2`)
- `sig_clip` ← `clip` (gulp `|x-median|>sigma` → median, `data_type=2`)
- `sig_blanker` ← `blanker` (constant `-P` period; no polyco)
- `sig_zerodm` ← `zerodm` (deterministic round-mean, recenter 64; no dither)
- `sig_reader` ← `reader` / `readchunk` (`-t/--time -w/--width`)
- `sig_filedit` ← `filedit` (in-place; `--dry-run`; same encoded length)

**Library in place (extend, do not replace):** `sigproc::io::SigprocHeader`,
`FilterbankReader` / `FilterbankWriter`, `sigproc::bits`, `sigproc::kernels`,
`sigproc::astro`, `sigproc::params`, `sigproc::fake`, `sigproc::TimeSeries`.

**Next (library-first, then CLI):** FBH5 (PR-19).
Dedisperse / fold / seek / RFI / tree wait for
[`docs/future-plan.md`](docs/future-plan.md).

**Library types to add when a CLI needs them** (sigpyproc3-shaped, C++):
`FilterbankBlock`, `TimeSeries`, `FourierSeries`, `FoldedCube`. Do not add them
speculatively.

## How to add a tool

1. Put the executable in `applications/sig_foo.cpp`. GLOB picks it up; do not
   edit `applications/CMakeLists.txt` to list the file.
2. Use CLI11. Reuse the library; do not reimplement header/IO/kernels in the
   app.
3. Match or exceed original SIGPROC behavior. Test format/numerics against the
   original where practical.
4. Do not pull in PGPLOT, PSRFITS, or historical backend converters.
