cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR
        "PICO_SDK_PATH is not set. Set it to your Raspberry Pi Pico SDK directory."
    )
endif()
set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Pico SDK location")
set(PICO_BOARD pico CACHE STRING "Generic RP2040 board; compatible with RP2040-Zero")
set(JOY_SETTINGS_OVERWRITE 0 CACHE BOOL "Restore embedded settings once on first boot")
set(JOY_SETTINGS_OVERWRITE_TOKEN 0 CACHE STRING "Token for one-time settings restore")

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(rp2040_zero_hid_joystick C CXX ASM)
pico_sdk_init()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/EmbedDefaultIni.cmake)
set(GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})
embed_default_ini(${CMAKE_CURRENT_LIST_DIR} ${GENERATED_INCLUDE_DIR})

add_executable(rp2040_zero_hid_joystick
    src/main.c
    src/ini_config.c
    src/joystick.c
    src/led_color.c
    src/msc_disk.c
    src/msc_volume.c
    src/settings.c
    src/status_led.c
    src/usb_descriptors.c
)

pico_generate_pio_header(rp2040_zero_hid_joystick ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(rp2040_zero_hid_joystick PRIVATE . src ${GENERATED_INCLUDE_DIR})
target_compile_definitions(rp2040_zero_hid_joystick PRIVATE
    CFG_TUSB_CONFIG_FILE="tusb_config.h"
    PICO_FLASH_ASSERT_ON_UNSAFE=0
    JOY_SETTINGS_OVERWRITE=${JOY_SETTINGS_OVERWRITE}
    JOY_SETTINGS_OVERWRITE_TOKEN=${JOY_SETTINGS_OVERWRITE_TOKEN}
)
target_link_libraries(rp2040_zero_hid_joystick
    pico_stdlib
    hardware_pio
    pico_bootrom
    pico_unique_id
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(rp2040_zero_hid_joystick)
