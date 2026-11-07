cmake_minimum_required(VERSION 3.15)
project(vl53l4cx_host_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

add_executable(test_platform test_platform.cpp ../vl53lx_platform_pico.cpp)
target_include_directories(test_platform PRIVATE
    mocks
    ..
    ../drivers/vl53l4cx/core/inc
    ../drivers/vl53l4cx/platform/inc
)
if(MSVC)
    target_compile_options(test_platform PRIVATE /W4 /WX)
else()
    target_compile_options(test_platform PRIVATE -Wall -Wextra -Werror)
endif()

foreach(test write_order read_order short_write short_read transport_errors invalid_arguments poll_ready poll_timeout timer)
    add_test(NAME ${test} COMMAND test_platform ${test})
endforeach()
