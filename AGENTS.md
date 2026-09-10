# Agent Guidelines for sigproc2

Rules for AI agents (and contributors) working in this repository. Read this
before making changes.

## Workflow & Git

- **Never commit to `main` directly.** Always work on a branch and open a Pull
  Request. The maintainer reviews every PR personally and merges it (and may
  request changes first). Do not merge PRs yourself.
- **Always commit under the maintainer's name:** `Pravir Kumar
  <pravirka@gmail.com>`.
- **Commit and PR messages:** keep them concise but describe all the changes.
  Write for humans — clear, readable, no filler.
- Make a separate commit for each logical change; do not force-push or amend
  unless asked.
- **When in doubt, ask.** If requirements or scope are unclear, ask the
  maintainer for direction rather than guessing.

## Code Style

- Follow the conventions already present in the code, and the checked-in
  `.clang-format` and `.clang-tidy` configurations.
- Naming (from `.clang-tidy`): classes/structs/enums `CamelCase`; functions,
  variables, parameters, members and namespaces `lower_case`; private/protected
  members prefixed `m_`; constants and `constexpr` values `CamelCase` prefixed
  `k` (e.g. `kDispConst`); type aliases `CamelCase`.
- Formatting (from `.clang-format`): LLVM base, 4-space indent, pointers bind
  left (`int* p`), one parameter per line when wrapping, aligned consecutive
  assignments. Run `clang-format` before committing.
- This is a modern **C++23** codebase: prefer `std::format`/`std::print`,
  `std::span`, concepts, and standard library facilities over hand-rolled or
  third-party equivalents. Keep public headers under `include/sigproc/`.

## Building & Testing

- Requirements: **GCC >= 15** (or Clang >= 19) and **CMake >= 3.30**, plus
  single-precision FFTW (with OpenMP) and Ninja. On a fresh machine these are
  installed by `.cursor/install.sh`.
- Configure and build:

  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```

- Enable the test suite with `-DBUILD_TESTING=ON` (Catch2). Enable docs with
  `-DBUILD_DOCS=ON`.
- Dependencies (spdlog, scnlib, CLI11, Catch2) are fetched via CPM; set
  `CPM_SOURCE_CACHE` to cache downloads across builds.

## Cloud Agent Environment

- `.cursor/environment.json` + `.cursor/install.sh` define the Cloud Agent
  environment so future branches build out of the box. Keep them in sync with
  the build requirements above.
