# Define the library from every translation unit under lib/ (GLOB, not an explicit list).
file(GLOB_RECURSE SIG_LIBRARY_CXX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${SIG_LIBRARY_NAME} ${SIG_LIBRARY_CXX_SOURCES})
add_library(${PROJECT_NAME}::sigproc ALIAS ${SIG_LIBRARY_NAME})

# Set library properties
set_target_properties(
  ${SIG_LIBRARY_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             CXX_STANDARD 23
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF
)

# Public API: include/sigproc (installed). Private headers: lib/sigproc (quoted, not installed).
target_include_directories(
  ${SIG_LIBRARY_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${SIG_LIBRARY_NAME} PRIVATE USE_OPENMP)

# Bundled CPM deps are build-tree only (not exported).
target_link_libraries(
  ${SIG_LIBRARY_NAME}
  PRIVATE $<BUILD_INTERFACE:${SIG_COMPILE_OPTIONS}> $<BUILD_INTERFACE:coverage_config>
          $<BUILD_INTERFACE:fmt::fmt-header-only> $<BUILD_INTERFACE:spdlog::spdlog_header_only>
          $<BUILD_INTERFACE:HighFive::HighFive> $<BUILD_INTERFACE:xsimd>
)

# OpenMP / HDF5 / FFTW: PRIVATE for shared; PUBLIC (FFTW LINK_ONLY) for static. See
# cmake/sigprocDependencies.cmake.
if(BUILD_SHARED_LIBS)
  target_link_libraries(${SIG_LIBRARY_NAME} PRIVATE OpenMP::OpenMP_CXX HDF5::HDF5 FFTW::Float)
else()
  target_link_libraries(${SIG_LIBRARY_NAME} PUBLIC OpenMP::OpenMP_CXX HDF5::HDF5)
  target_link_libraries(${SIG_LIBRARY_NAME} PUBLIC $<LINK_ONLY:FFTW::Float>)
endif()
