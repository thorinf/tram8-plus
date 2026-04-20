cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "")

project(tram8-bridge
  VERSION 0.1.0
  LANGUAGES CXX OBJCXX
)

add_subdirectory(external)
smtg_enable_vst3_sdk()

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

target_link_libraries(tram8-bridge
  PRIVATE
    sdk
)

if(SMTG_MAC)
  target_link_libraries(tram8-bridge
    PRIVATE
      "-framework CoreMIDI"
      "-framework CoreFoundation"
      "-framework Cocoa"
      "-framework WebKit"
  )
  smtg_target_set_bundle(tram8-bridge
    BUNDLE_IDENTIFIER com.thorinf.tram8bridge
    COMPANY_NAME "thorinf"
  )
  smtg_target_add_plugin_resources(tram8-bridge
    RESOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/resource/ui.html"
  )
endif()
