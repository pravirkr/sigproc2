CPMFindPackage(
  NAME Catch2
  VERSION 3.4.0
  GITHUB_REPOSITORY catchorg/Catch2
)

if(Catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
elseif(Catch2_DIR)
  list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
endif()

file(GLOB SIG_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(tests ${SIG_TEST_SOURCES})
target_link_libraries(
  tests PRIVATE ${SIG_LIBRARY_NAME} Catch2::Catch2WithMain ${SIG_COMPILE_OPTIONS} coverage_config
)
target_compile_definitions(
  tests PRIVATE SIG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
                SIG_BIN_DIR="${CMAKE_BINARY_DIR}/applications"
)

include(CTest)
include(Catch)
catch_discover_tests(tests)
