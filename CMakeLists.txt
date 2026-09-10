cmake_minimum_required(VERSION 3.30)

project(
  sigproc
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "IO and processing library for pulsar data"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler version requirements
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
    message(
      FATAL_ERROR
        "GCC >= 15.0 required for complete C++23 support. Found: ${CMAKE_CXX_COMPILER_VERSION}"
    )
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.0)
    message(
      FATAL_ERROR
        "Clang >= 19.0 required for complete C++23 support. Found: ${CMAKE_CXX_COMPILER_VERSION}"
    )
  endif()
endif()

# Configuration options
option(BUILD_DOCS "Build documentation" OFF)
option(BUILD_TESTING "Build tests" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_IPO "Enable Interprocedural Optimization (Link-Time Optimization)" OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Required for shared libraries

# Set default build type
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

# Interprocedural Optimization (IPO/LTO)
if(CMAKE_BUILD_TYPE MATCHES "Release" AND ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "IPO/LTO enabled for Release build.")
  else()
    message(WARNING "IPO/LTO requested but not supported.")
  endif()
else()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

# Project structure and paths
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(GNUInstallDirs) # Defines standard installation paths

# Compiler flags (Interface target to hold global compile options)
set(SIG_COMPILE_OPTIONS ${PROJECT_NAME}_COMPILE_OPTIONS)
add_library(${SIG_COMPILE_OPTIONS} INTERFACE)
target_compile_options(${SIG_COMPILE_OPTIONS} INTERFACE -Wall -Wextra -Wpedantic)
target_compile_options(
  ${SIG_COMPILE_OPTIONS}
  INTERFACE $<$<CONFIG:Debug>:-g
            -O0
            -fsanitize=address
            -fsanitize=undefined>
            $<$<CONFIG:Release>:-O3;-DNDEBUG>
            $<$<CONFIG:MinSizeRel>:-Os;-DNDEBUG>
            $<$<AND:$<NOT:$<CONFIG:Debug>>,$<COMPILE_LANGUAGE:CXX>>:-march=native;-ffast-math>
)

# CPM.cmake Setup
include(cmake/CPM.cmake)
set(CPM_USE_LOCAL_PACKAGES
    ON
    CACHE BOOL "Try to find packages locally before downloading" FORCE
)
if(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_SOURCE_CACHE
      $ENV{CPM_SOURCE_CACHE}
      CACHE PATH "Directory to cache downloaded CPM sources"
  )
endif()

# Define library names and Add subdirectories
set(SIG_LIBRARY_NAME ${PROJECT_NAME})
add_subdirectory(lib)
add_subdirectory(applications)
if(BUILD_TESTING)
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

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate and install export file
install(
  EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Create and install config file
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
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Feature summary
include(FeatureSummary)
feature_summary(WHAT ALL)
