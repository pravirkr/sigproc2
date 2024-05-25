CPMFindPackage(
  NAME Catch2
  VERSION 3.5.0
  GITHUB_REPOSITORY catchorg/Catch2
)
list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

add_executable(tests tests.cpp)
target_link_libraries(tests PUBLIC sigproc CATCH2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tests)
