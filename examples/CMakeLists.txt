add_executable(basic_shapes basic_shapes/main.cpp)
target_link_libraries(basic_shapes PRIVATE RoadOS::graphics roados_warnings)

add_executable(svg_rendering svg_rendering/main.cpp)
target_link_libraries(svg_rendering PRIVATE RoadOS::graphics roados_warnings)
