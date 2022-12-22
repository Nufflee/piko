cmake_minimum_required(VERSION 3.13)

# Skip compiler check, there seems to be an issue with Cmake 3.22 (https://stackoverflow.com/a/54252398)
#set(CMAKE_C_COMPILER_WORKS ON)
#set(CMAKE_CXX_COMPILER_WORKS ON)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

set(CMAKE_CXX_STANDARD 17)

#enable_language(C CXX ASM)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(piko)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_compile_options(-Wall -Wextra)

add_executable(piko main.cpp src/piko.cpp)

target_compile_definitions(piko PRIVATE
                           PICO_DIVIDER_DISABLE_INTERRUPTS=1)

target_compile_options(piko PRIVATE -Wall)

target_link_libraries(piko
                      pico_stdlib
                      hardware_exception)

pico_add_extra_outputs(piko)
pico_enable_stdio_uart(piko 1)
pico_enable_stdio_usb(piko 1)