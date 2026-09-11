cmake_minimum_required(VERSION 3.28)

project(
  sigproc
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "IO and processing library for pulsar data"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler version requirements
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.2)
    message(
      FATAL_ERROR
        "GCC >= 14.2 required for complete C++23 support. Found: ${CMAKE_CXX_COMPILER_VERSION}"
    )
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
    message(
      FATAL_ERROR
        "Clang >= 18.0 required for complete C++23 support. Found: ${CMAKE_CXX_COMPILER_VERSION}"
    )
  endif()
else()
  message(
    FATAL_ERROR
      "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. Use GCC >= 14.2 or LLVM Clang >= 18.0."
  )
endif()

# Configuration options
option(SIG_BUILD_TESTING "Build C++ tests" OFF)
option(SIG_ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(SIG_ENABLE_IPO "Enable interprocedural optimization (LTO) on Release builds" OFF)
option(SIG_ENABLE_NATIVE_ARCH "Use -march=native in Release builds" ON)
option(SIG_USE_SYSTEM_DEPS "Prefer compatible system packages over pinned bundled dependencies" OFF)
# BUILD_SHARED_LIBS is a standard CMake option (ON = shared sigproc, OFF = static sigproc).
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Required for shared libraries

# Default to Release when no build type is specified (packaging-friendly).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build: Debug, Release, MinSizeRel, RelWithDebInfo." FORCE
  )
  set_property(
    CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
  )
endif()

# Optional LTO for Release builds.
if(CMAKE_BUILD_TYPE MATCHES "Release" AND SIG_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "IPO/LTO enabled for Release build.")
  else()
    message(FATAL_ERROR "SIG_ENABLE_IPO=ON but IPO/LTO is not supported by this toolchain.")
  endif()
else()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(GNUInstallDirs)

# Internal compile-option interface (build tree only — not exported on install).
# Interface target: link any library/executable to this to inherit these flags.
# Usage: target_link_libraries(my_target PRIVATE ${SIG_COMPILE_OPTIONS})
set(SIG_COMPILE_OPTIONS ${PROJECT_NAME}_compile_options)
add_library(${SIG_COMPILE_OPTIONS} INTERFACE)
target_compile_options(${SIG_COMPILE_OPTIONS} INTERFACE -Wall -Wextra -Wpedantic)
target_compile_options(
  ${SIG_COMPILE_OPTIONS}
  INTERFACE $<$<CONFIG:Debug>:-O0;-g> $<$<CONFIG:Release>:-O3;-DNDEBUG;-ffast-math>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${SIG_ENABLE_NATIVE_ARCH}>>:-march=native>
            $<$<CONFIG:RelWithDebInfo>:-O2;-DNDEBUG;-g> $<$<CONFIG:MinSizeRel>:-Os;-DNDEBUG>
)

# Code coverage (build tree only).
add_library(coverage_config INTERFACE)
if(SIG_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    message(STATUS "Code coverage enabled.")
  else()
    message(
      WARNING "Code coverage requested, but not supported for compiler ${CMAKE_CXX_COMPILER_ID}."
    )
    set(SIG_ENABLE_COVERAGE
        OFF
        CACHE BOOL "" FORCE
    )
  endif()
endif()

# CPM: bundled deps by default; SIG_USE_SYSTEM_DEPS=ON prefers find_package first.
include(cmake/CPM.cmake)
set(CPM_USE_LOCAL_PACKAGES
    ${SIG_USE_SYSTEM_DEPS}
    CACHE BOOL "Try to find packages locally before downloading" FORCE
)
if(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_SOURCE_CACHE
      $ENV{CPM_SOURCE_CACHE}
      CACHE PATH "Directory to cache downloaded CPM sources"
  )
endif()

include(sigprocDependencies)

# Generated public version header
configure_file(
  ${PROJECT_SOURCE_DIR}/version.hpp.in ${CMAKE_BINARY_DIR}/include/sigproc/version.hpp @ONLY
)

# Define library names and Add subdirectories
set(SIG_LIBRARY_NAME ${PROJECT_NAME})
add_subdirectory(lib)
add_subdirectory(applications)
if(SIG_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# Install targets
install(
  TARGETS ${SIG_LIBRARY_NAME}
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers only (include/sigproc). Private headers live under lib/sigproc/.
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN "*.in" EXCLUDE
)
install(FILES ${CMAKE_BINARY_DIR}/include/sigproc/version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sigproc
)

# Generate and install export file
install(
  EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Create and install config file
set(SIGPROC_BUILT_AS_SHARED ${BUILD_SHARED_LIBS})
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)
write_basic_package_version_file(
  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(
  FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        ${PROJECT_SOURCE_DIR}/cmake/FindFFTW.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Feature summary
include(FeatureSummary)
feature_summary(WHAT ALL)
