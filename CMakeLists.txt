cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME "$ENV{USERPROFILE}")
else()
    set(USERHOME "$ENV{HOME}")
endif()

set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode "${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake")
if(EXISTS "${picoVscode}")
    include("${picoVscode}")
endif()

set(PICO_BOARD pico2_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(VL53L4CX-Distance-Sensor C CXX ASM)
pico_sdk_init()

file(GLOB VL53L4CX_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/drivers/vl53l4cx/core/src/*.c"
)

add_executable(VL53L4CX-Distance-Sensor
    VL53L4CX-Distance-Sensor.cpp
    vl53lx_platform_pico.cpp
    drivers/vl53l4cx/platform/src/vl53lx_platform_ipp.c
    ${VL53L4CX_CORE_SOURCES}
)

set_source_files_properties(VL53L4CX-Distance-Sensor.cpp vl53lx_platform_pico.cpp PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)

target_include_directories(VL53L4CX-Distance-Sensor PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/vl53l4cx/core/inc"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/vl53l4cx/platform/inc"
)

target_compile_options(VL53L4CX-Distance-Sensor PRIVATE -fstack-usage)
target_compile_definitions(VL53L4CX-Distance-Sensor PRIVATE
    PICO_STACK_SIZE=0x1000
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=10000
)

pico_set_program_name(VL53L4CX-Distance-Sensor "VL53L4CX-Distance-Sensor")
pico_set_program_version(VL53L4CX-Distance-Sensor "1.0")
pico_enable_stdio_uart(VL53L4CX-Distance-Sensor 0)
pico_enable_stdio_usb(VL53L4CX-Distance-Sensor 1)

target_link_libraries(VL53L4CX-Distance-Sensor pico_stdlib hardware_i2c)
pico_add_extra_outputs(VL53L4CX-Distance-Sensor)
