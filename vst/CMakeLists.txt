cmake_minimum_required(VERSION 3.25.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" TRAM8_VERSION)
string(STRIP "${TRAM8_VERSION}" TRAM8_VERSION)
if(NOT TRAM8_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "Invalid VERSION file contents: '${TRAM8_VERSION}' (expected MAJOR.MINOR.PATCH)")
endif()

project(tram8-bridge
  VERSION ${TRAM8_VERSION}
  LANGUAGES CXX OBJCXX
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/source/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
  @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
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
    -DINPUT=${UI_HTML_INPUT}
    -DOUTPUT=${UI_HTML_OUTPUT}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_html.cmake
  DEPENDS "${UI_HTML_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_html.cmake"
  VERBATIM
  COMMENT "Generating ui_html.h from ui.html"
)
add_custom_target(generate_ui_html DEPENDS "${UI_HTML_OUTPUT}")

smtg_add_vst3plugin(tram8-bridge
  source/cids.h
  source/midi_engine.h
  source/midi_engine.cpp
  source/state_format.h
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

if(APPLE)
  add_executable(tram8-editor-sync-test EXCLUDE_FROM_ALL
    tests/test_editor_sync.mm
    source/controller.cpp
    source/plugview.mm
    external/vst3sdk/public.sdk/source/common/memorystream.cpp
  )
  add_dependencies(tram8-editor-sync-test generate_ui_html)
  target_include_directories(tram8-editor-sync-test PRIVATE source "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_link_libraries(tram8-editor-sync-test PRIVATE sdk sdk_hosting
    "-framework Cocoa" "-framework WebKit" "-framework CoreMIDI")
  target_compile_options(tram8-editor-sync-test PRIVATE -Wall -Wextra -Wno-unused-parameter -UNDEBUG)
  set_target_properties(tram8-editor-sync-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
  add_executable(tram8-webview-bridge-test EXCLUDE_FROM_ALL
    tests/test_webview_bridge.mm source/controller.cpp source/plugview.mm)
  add_dependencies(tram8-webview-bridge-test generate_ui_html)
  target_include_directories(tram8-webview-bridge-test PRIVATE source "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_link_libraries(tram8-webview-bridge-test PRIVATE sdk sdk_hosting
    "-framework Cocoa" "-framework WebKit" "-framework CoreMIDI")
  target_compile_options(tram8-webview-bridge-test PRIVATE -Wall -Wextra -Wno-unused-parameter -UNDEBUG)
  set_target_properties(tram8-webview-bridge-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
  add_executable(test_midi_port EXCLUDE_FROM_ALL
    tests/test_midi_port.cpp
    source/processor.cpp
    source/midi_engine.cpp
  )
  target_include_directories(test_midi_port PRIVATE source)
  target_link_libraries(test_midi_port PRIVATE sdk sdk_hosting
    "-framework CoreMIDI" "-framework CoreFoundation")
  target_compile_definitions(test_midi_port PRIVATE RELEASE=1)
  target_compile_options(test_midi_port PRIVATE -UNDEBUG)
  set_target_properties(test_midi_port PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()
