# Define the library
file(GLOB SIG_LIBRARY_CXX_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
set(SIG_LIBRARY_ALL_SOURCES ${SIG_LIBRARY_CXX_SOURCES})

add_library(${SIG_LIBRARY_NAME} ${SIG_LIBRARY_ALL_SOURCES})
add_library(${PROJECT_NAME}::sigproc ALIAS ${SIG_LIBRARY_NAME})

# Set library properties
set_target_properties(
  ${SIG_LIBRARY_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF
)

# System dependencies
message(STATUS "Searching for required system dependencies...")
find_package(OpenMP REQUIRED)
find_package(FFTW REQUIRED COMPONENTS FLOAT_LIB FLOAT_OPENMP_LIB)

# CPM Dependencies
message(STATUS "Searching for CPM dependencies (will download if not found locally)...")

# spdlog (Header-only logging library)
CPMAddPackage(
  NAME spdlog
  VERSION 1.15.3
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
  OPTIONS "SPDLOG_INSTALL ON"
)

# scnlib
CPMAddPackage(
  NAME scn
  VERSION 4.0.1
  URL https://github.com/eliaskosunen/scnlib/archive/refs/tags/v4.0.1.tar.gz
  OPTIONS "SCN_DISABLE_FAST_FLOAT ON" "SCN_INSTALL OFF"
)

# Code coverage setup
if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_library(coverage_config INTERFACE)
    target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
    target_link_options(coverage_config INTERFACE --coverage)
    message(STATUS "Code coverage enabled.")
  else()
    message(
      WARNING "Code coverage requested, but not supported for compiler ${CMAKE_CXX_COMPILER_ID}."
    )
    set(ENABLE_COVERAGE
        OFF
        CACHE BOOL "" FORCE
    )
    add_library(coverage_config INTERFACE)
  endif()
else()
  add_library(coverage_config INTERFACE)
endif()

configure_file(${PROJECT_SOURCE_DIR}/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.hpp)

# Set include directories
target_include_directories(
  ${SIG_LIBRARY_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)

# Link libraries
# Private dependencies (build-time only, not exported)
target_link_libraries(
  ${SIG_LIBRARY_NAME} PRIVATE $<BUILD_INTERFACE:${SIG_COMPILE_OPTIONS}> $<BUILD_INTERFACE:scn::scn>
                              $<BUILD_INTERFACE:coverage_config>
)

# Public dependencies (exported to consumers)
target_link_libraries(
  ${SIG_LIBRARY_NAME} PUBLIC OpenMP::OpenMP_CXX FFTW::Float FFTW::FloatOpenMP
                             spdlog::spdlog_header_only
)
