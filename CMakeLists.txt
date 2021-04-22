cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)

project(
  sigproc
  VERSION 0.1.0
  LANGUAGES CXX)

option(ENABLE_CLANG_TIDY "Run Clang Tidy to get static analysis" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FindGit)
include(FetchContent)

find_package(Git)
if(NOT Git_FOUND)
  message(FATAL_ERROR "Git not found!")
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS date_time)
include_directories(${Boost_INCLUDE_DIRS})

# search for CMake configuration file for CLI11
find_package(CLI11 1.9.1 CONFIG QUIET)
if(CLI11_FOUND)
  message(STATUS "Found {CLI11}: ${CLI11_DIR} (found version '${CLI11_VERSION}')")
else(CLI11_FOUND)
  message(STATUS "No local {CLI11} installation found. Downloading from Git...")
  FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG 1.9.1)
  FetchContent_GetProperties(CLI11)
  if(NOT CLI11_POPULATED)
    FetchContent_Populate(CLI11)
    message(STATUS "{CLI11} source dir: ${CLI11_SOURCE_DIR}")
    add_subdirectory(${CLI11_SOURCE_DIR} ${CLI11_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
  message(STATUS "{CLI11} include dir: ${CLI11_SOURCE_DIR}/include")
endif(CLI11_FOUND)

# search for CMake configuration file for fmt
find_package(fmt 7.1.3 QUIET)
if(fmt_FOUND)
  message(STATUS "Found {fmt}: ${fmt_DIR} (found version '${fmt_VERSION}')")
else(fmt_FOUND)
  message(STATUS "No local {fmt} installation found. Downloading from Git...")
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 7.1.3)
  FetchContent_GetProperties(fmt)
  if(NOT fmt_POPULATED)
    FetchContent_Populate(fmt)
    message(STATUS "{fmt} source dir: ${fmt_SOURCE_DIR}")
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
  message(STATUS "{fmt} include dir: ${fmt_SOURCE_DIR}/include")
endif(fmt_FOUND)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

configure_file(version.hpp.in version.hpp)


# Set variables
set(sigproc_headers
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src)

set(external_headers
  ${CLI11_SOURCE_DIR}/include
  ${fmt_SOURCE_DIR}/include)

set(src_files
  ${PROJECT_SOURCE_DIR}/src/fileIO.cpp
  ${PROJECT_SOURCE_DIR}/src/pack_unpack.cpp)


set(ALL_LIBRARIES
  ${Boost_LIBRARIES}
  m
  CLI11::CLI11
  fmt::fmt-header-only)


include_directories(${PROJECT_NAME} PRIVATE ${sigproc_headers} )
include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${external_headers} )


add_library(libsigproc SHARED ${src_files})
target_link_libraries(libsigproc PRIVATE ${ALL_LIBRARIES})

add_executable(header ${PROJECT_SOURCE_DIR}/applications/header.cpp)
target_link_libraries(header PRIVATE libsigproc ${ALL_LIBRARIES})

add_executable(decimate ${PROJECT_SOURCE_DIR}/applications/decimate.cpp)
target_link_libraries(decimate PRIVATE libsigproc ${ALL_LIBRARIES})

add_executable(bandpass ${PROJECT_SOURCE_DIR}/applications/bandpass.cpp)
target_link_libraries(bandpass PRIVATE libsigproc ${ALL_LIBRARIES})

add_executable(chop_fil ${PROJECT_SOURCE_DIR}/applications/chop_fil.cpp)
target_link_libraries(chop_fil PRIVATE libsigproc ${ALL_LIBRARIES})

# ------------------------------------------------------------------------------
# Clang Tidy
# ------------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_BIN clang-tidy)
  find_program(RUN_CLANG_TIDY_BIN run-clang-tidy)

  if(CLANG_TIDY_BIN STREQUAL "CLANG_TIDY_BIN-NOTFOUND")
    message(FATAL_ERROR "unable to locate clang-tidy")
  endif()

  if(RUN_CLANG_TIDY_BIN STREQUAL "RUN_CLANG_TIDY_BIN-NOTFOUND")
    message(FATAL_ERROR "unable to locate run-clang-tidy")
  endif()

  list(APPEND RUN_CLANG_TIDY_BIN_ARGS -clang-tidy-binary ${CLANG_TIDY_BIN})

  add_custom_target(
    tidy
    COMMAND ${RUN_CLANG_TIDY_BIN} ${RUN_CLANG_TIDY_BIN_ARGS}
    COMMENT "running clang tidy")

endif()
