file(GLOB SIG_APPLICATION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(SIG_APPLICATIONS)
foreach(SOURCE_FILE IN LISTS SIG_APPLICATION_SOURCES)
  get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
  add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
  target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE ${SIG_LIBRARY_NAME}
            CLI11::CLI11
            ${SIG_COMPILE_OPTIONS}
            coverage_config
            $<BUILD_INTERFACE:fmt::fmt-header-only>
            $<BUILD_INTERFACE:spdlog::spdlog_header_only>
  )
  list(APPEND SIG_APPLICATIONS ${EXECUTABLE_NAME})
endforeach()

install(TARGETS ${SIG_APPLICATIONS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
