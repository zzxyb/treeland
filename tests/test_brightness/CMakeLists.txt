find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_brightness main.cpp)
target_include_directories(test_brightness PRIVATE "${PROJECT_SOURCE_DIR}/src/output")
target_link_libraries(test_brightness PRIVATE libtreeland Qt6::Core Qt6::Test)
add_test(NAME test_brightness COMMAND test_brightness)
set_tests_properties(test_brightness PROPERTIES TIMEOUT 10)
