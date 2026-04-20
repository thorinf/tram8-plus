cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(tram8-bridge
  VERSION 0.1.0
  LANGUAGES CXX OBJCXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Allow building with CommandLineTools (no full Xcode required)
if(APPLE AND NOT DEFINED XCODE_VERSION AND NOT DEFINED ENV{XCODE_VERSION})
  execute_process(
    COMMAND xcrun clang --version
    OUTPUT_VARIABLE _clang_out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(_clang_out)
    string(REGEX MATCH "[0-9]+" _clang_major "${_clang_out}")
    set(XCODE_VERSION "${_clang_major}.0")
    set(ENV{XCODE_VERSION} "${XCODE_VERSION}")
  endif()
endif()

add_subdirectory(external)
smtg_enable_vst3_sdk()

set(UI_HTML_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/source/ui.html")
set(UI_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/ui_html.h")

add_custom_command(
  OUTPUT "${UI_HTML_OUTPUT}"
  COMMAND ${CMAKE_COMMAND}
    -DINPUT="${UI_HTML_INPUT}"
    -DOUTPUT="${UI_HTML_OUTPUT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_html.cmake"
  DEPENDS "${UI_HTML_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_html.cmake"
  VERBATIM
  COMMENT "Generating ui_html.h from ui.html"
)
add_custom_target(generate_ui_html DEPENDS "${UI_HTML_OUTPUT}")

smtg_add_vst3plugin(tram8-bridge
  source/cids.h
  source/version.h
  source/processor.h
  source/processor.cpp
  source/controller.h
  source/controller.cpp
  source/plugview.h
  source/plugview.mm
  source/entry.cpp
)

add_dependencies(tram8-bridge generate_ui_html)
target_include_directories(tram8-bridge PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(tram8-bridge
  PRIVATE
    sdk
)

include(cmake/warnings.cmake)
include(cmake/platform.cmake)
