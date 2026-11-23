include(FetchContent)
# GoogleTest 1.17.0 official release tarball, pinned by SHA-256.
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(roados_unit_tests
  unit/ColorTest.cpp
  unit/TransformTest.cpp
  unit/PathTest.cpp
  unit/ShapeTest.cpp
  unit/SceneTest.cpp
  unit/JsFormatTest.cpp
)
target_link_libraries(roados_unit_tests PRIVATE RoadOS::rvhl GTest::gtest_main roados_warnings)
target_include_directories(roados_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
gtest_discover_tests(roados_unit_tests PROPERTIES LABELS unit)

set(ROADOS_TEST_DEFINES
  ROADOS_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
  ROADOS_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
  ROADOS_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")

add_executable(roados_rendering_tests
  rendering/RenderTest.cpp
  rendering/ImageTest.cpp
  rendering/VisualRegressionTest.cpp
  rendering/HazardRenderTest.cpp
)
target_link_libraries(roados_rendering_tests PRIVATE RoadOS::rvhl GTest::gtest_main roados_warnings)
target_include_directories(roados_rendering_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(roados_rendering_tests PRIVATE ${ROADOS_TEST_DEFINES})
gtest_discover_tests(roados_rendering_tests PROPERTIES LABELS rendering)

add_executable(roados_integration_tests
  integration/SvgTest.cpp
  integration/RvhlAssetsTest.cpp
  integration/CatalogueTest.cpp
  integration/ComposeParityTest.cpp
  integration/SeverityParityTest.cpp
  integration/BindingTest.cpp
)
target_link_libraries(roados_integration_tests PRIVATE RoadOS::binding GTest::gtest_main roados_warnings)
target_include_directories(roados_integration_tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/nlohmann)
target_compile_definitions(roados_integration_tests PRIVATE ${ROADOS_TEST_DEFINES})
gtest_discover_tests(roados_integration_tests PROPERTIES LABELS integration)
