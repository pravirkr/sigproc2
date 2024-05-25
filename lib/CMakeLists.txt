file(GLOB LIBRARY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(${LIBRARY_NAME} ${LIBRARY_SOURCES})
target_include_directories(
  ${LIBRARY_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/>
                         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(${LIBRARY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  ${LIBRARY_NAME}
  PUBLIC $<BUILD_INTERFACE:coverage_config> $<BUILD_INTERFACE:spdlog::spdlog_header_only>
         ${Boost_LIBRARIES} ${FFTW_FLOAT_LIB}
)

# Check if OpenMP was found
if(OpenMP_CXX_FOUND)
  target_link_libraries(${LIBRARY_NAME} PRIVATE OpenMP::OpenMP_CXX)
  target_compile_definitions(${LIBRARY_NAME} PRIVATE USE_OPENMP)
endif()
