cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "")

project(tram8-bridge
  VERSION 0.1.0
  LANGUAGES CXX OBJCXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/warnings.cmake)

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

include(cmake/platform.cmake)
