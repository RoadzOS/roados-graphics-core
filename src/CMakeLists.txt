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

# B. RoadOS RVHL layer: catalogue, composition, severity. Uses only the public
# graphics API (no Skia). nlohmann/json is a private, vendored dependency.
add_library(roados_rvhl STATIC
  rvhl/Catalogue.cpp
  rvhl/Compose.cpp
  rvhl/Hazard.cpp
  rvhl/JsFormat.cpp
  rvhl/Severity.cpp
  rvhl/Sha256.cpp
)
add_library(RoadOS::rvhl ALIAS roados_rvhl)
target_include_directories(roados_rvhl
  PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(roados_rvhl SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/nlohmann)
target_link_libraries(roados_rvhl PUBLIC roados_graphics PRIVATE roados_warnings)

# C. Binding facade for platform SDKs (JNI, Objective-C++): plain-data specs,
# a scene builder and a shared catalogue cache. Bridges only marshal into it.
add_library(roados_binding STATIC binding/Binding.cpp)
add_library(RoadOS::binding ALIAS roados_binding)
target_include_directories(roados_binding PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries(roados_binding PUBLIC roados_rvhl PRIVATE roados_warnings)
