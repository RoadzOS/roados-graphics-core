cmake_minimum_required(VERSION 3.24)

project(RoadOSGraphicsCore
  VERSION 0.1.0
  DESCRIPTION "Reusable native vector graphics engine powered by Skia"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ROADOS_BUILD_TESTS "Build unit, integration and rendering tests" ON)
option(ROADOS_BUILD_EXAMPLES "Build example programs" ON)
option(ROADOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(Skia REQUIRED)

add_library(roados_warnings INTERFACE)
if(MSVC)
  target_compile_options(roados_warnings INTERFACE /W4 $<$<BOOL:${ROADOS_WARNINGS_AS_ERRORS}>:/WX>)
else()
  target_compile_options(roados_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
    $<$<BOOL:${ROADOS_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

add_subdirectory(src)

if(ROADOS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ROADOS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(ROADOS_BUILD_TOOLS "Build developer tools (image_diff)" ON)
if(ROADOS_BUILD_TOOLS)
  add_subdirectory(tools)
endif()
