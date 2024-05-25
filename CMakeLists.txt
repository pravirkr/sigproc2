cmake_minimum_required(VERSION 3.18)

project(
  sigproc
  VERSION 0.1.0
  LANGUAGES CXX
)

option(BUILD_DOCS "Build documentation" OFF)
option(BUILD_TESTING "Build tests" OFF)

# Define the minimum C++ standard that is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -ffast-math")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Initialize some default paths
include(GNUInstallDirs)
include(cmake/CPM.cmake)

# Find dependencies
CPMFindPackage(
  NAME spdlog
  VERSION 1.10.0
  GITHUB_REPOSITORY gabime/spdlog
  OPTIONS "SPDLOG_INSTALL ON"
)
find_package(FFTW REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS date_time)

configure_file(version.hpp.in version.hpp)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PROJECT_NAME} PRIVATE ${FFTW_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
# Build the c++ library
set(LIBRARY_NAME sigproc)
add_subdirectory(lib)
add_subdirectory(applications)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
