# A. General-purpose graphics engine (no RoadOS knowledge).
add_library(roados_graphics STATIC
  core/Core.cpp
  paths/Path.cpp
  shapes/Shapes.cpp
  renderer/SkiaConvert.cpp
  renderer/Image.cpp
  renderer/Renderer.cpp
  svg/Svg.cpp
)
add_library(RoadOS::graphics ALIAS roados_graphics)

target_include_directories(roados_graphics
  PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Skia is a PRIVATE dependency: public headers never include it, and Skia's
# defines never leak into consumers. CMake still propagates the static link.
target_link_libraries(roados_graphics PRIVATE Skia::Skia roados_warnings)

target_compile_definitions(roados_graphics PRIVATE
  ROADOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  ROADOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  ROADOS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  ROADOS_SKIA_REVISION="${Skia_REVISION}")
