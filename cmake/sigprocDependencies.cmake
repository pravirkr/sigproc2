# =============================================================================
# SIGPROC — Dependency management (CPU + shared CPM deps)
#
# Design rules:
#   1. OpenMP, HDF5, and FFTW are hard system dependencies — never fetched.
#   2. FFTW is linked PRIVATE for shared sigproc;
#      for static sigproc it is linked with LINK_ONLY so consumers resolve symbols
#      without inheriting fftw3.h include paths.
#   3. OpenMP and HDF5 are internal. For a SHARED sigproc they are linked PRIVATE;
#      for STATIC sigproc they are linked PUBLIC so consumers can resolve symbols.
#   4. fmt and spdlog are CPM-pinned header-only libraries (single translation
#      unit, no version skew with libc++ std::format instantiations).
#   5. CLI11, xsimd and HighFive are
#      header-only internal dependencies -> BUILD_INTERFACE only.
# =============================================================================

# -----------------------------------------------------------------------
# 1. System dependencies — must be supplied by the user, never fetched
# -----------------------------------------------------------------------
message(STATUS "Searching for required system dependencies...")
find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(HDF5 REQUIRED) # For HighFive
find_package(FFTW REQUIRED COMPONENTS FLOAT_LIB)

# -----------------------------------------------------------------------
# 2. CPM-managed dependencies (pinned versions, bundled by default)
# -----------------------------------------------------------------------
if(SIG_USE_SYSTEM_DEPS)
  message(STATUS "SIG_USE_SYSTEM_DEPS=ON: compatible system packages may be used.")
else()
  message(STATUS "SIG_USE_SYSTEM_DEPS=OFF: using pinned CPM dependencies.")
endif()

# fmt — keep version in sync with spdlog's expected fmt release.
CPMAddPackage(
  NAME fmt
  VERSION 12.1.0
  URL https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
  OPTIONS "FMT_INSTALL ON" "FMT_HEADER_ONLY ON"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

# spdlog — use the fmt package above (header-only external fmt).
CPMAddPackage(
  NAME spdlog
  VERSION 1.17.0
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
  OPTIONS "SPDLOG_INSTALL ON" "SPDLOG_FMT_EXTERNAL_HO ON" "SPDLOG_BUILD_SHARED OFF"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

if(NOT TARGET fmt::fmt-header-only)
  message(FATAL_ERROR "fmt::fmt-header-only target not available after CPM setup.")
endif()
if(NOT TARGET spdlog::spdlog_header_only)
  message(FATAL_ERROR "spdlog::spdlog_header_only target not available after CPM setup.")
endif()

# -----------------------------------------------------------------------
# 3. Header-only CPM dependencies (build-time only)
# -----------------------------------------------------------------------

CPMAddPackage(
  NAME HighFive
  VERSION 3.3.0
  URL https://github.com/highfive-devs/highfive/archive/refs/tags/v3.3.0.tar.gz
  OPTIONS "HIGHFIVE_FIND_HDF5 ON"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
if(NOT TARGET HighFive::HighFive)
  message(FATAL_ERROR "HighFive::HighFive target not available after CPM setup.")
endif()

# xsimd (SIMD library) - Header-only
CPMAddPackage(
  NAME xsimd
  VERSION 14.0.0
  URL https://github.com/xtensor-stack/xsimd/archive/refs/tags/14.0.0.tar.gz
  DOWNLOAD_ONLY YES
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
if(xsimd_ADDED)
  add_library(xsimd INTERFACE IMPORTED GLOBAL)
  target_include_directories(xsimd INTERFACE "${xsimd_SOURCE_DIR}/include")
elseif(NOT TARGET xsimd)
  message(FATAL_ERROR "xsimd is required but was not found or downloaded.")
endif()

CPMAddPackage(
  NAME CLI11
  VERSION 2.7.0
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.7.0.tar.gz
  DOWNLOAD_ONLY YES
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)
if(CLI11_ADDED)
  add_library(CLI11::CLI11 INTERFACE IMPORTED GLOBAL)
  target_include_directories(CLI11::CLI11 INTERFACE "${CLI11_SOURCE_DIR}/include")
elseif(NOT TARGET CLI11::CLI11)
  message(FATAL_ERROR "CLI11 is required but was not found or downloaded.")
endif()
