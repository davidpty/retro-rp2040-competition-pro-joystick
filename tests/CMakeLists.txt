cmake_minimum_required(VERSION 3.13)
project(joystick_host_tests C)
enable_testing()

add_executable(joystick_host_tests test_joystick.c ../src/joystick.c ../src/settings.c)
target_include_directories(joystick_host_tests PRIVATE .. ../src)
target_compile_features(joystick_host_tests PRIVATE c_std_11)
target_compile_options(joystick_host_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME joystick_logic COMMAND joystick_host_tests)
