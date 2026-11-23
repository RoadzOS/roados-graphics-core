add_executable(image_diff image_diff/main.cpp)
target_link_libraries(image_diff PRIVATE RoadOS::graphics roados_warnings)
