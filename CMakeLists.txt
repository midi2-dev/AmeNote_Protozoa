cmake_minimum_required(VERSION 3.12)

#############################################
# Pull in Pico SDK (must be before project)

# inject tinyusb dependency
# NO LONGER NEEDED as using APP specific driver routines. See ump_driver files.
# set (PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb")

include(pico_sdk_import.cmake)


#############################################

# Pull in FreeRTOS-Kernel (must be before pico_sdk_init() )
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel")
include(FreeRTOS_Kernel_import.cmake)
message( "!!!FreeRTOS Path is: ${FREERTOS_KERNEL_PATH}")

project(AmeNote C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.1")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Initialize the SDK
pico_sdk_init()

# include(example_auto_set_url.cmake)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

add_subdirectory(lib/AM_MIDI2.0Lib)
add_subdirectory(ProtoZOA_Main)
add_subdirectory(ProtoZOA_PicoProbe)


add_subdirectory(UUT/DIN_Bridge)
add_subdirectory(UUT/CME_WIDI_CORE_EXP)
add_subdirectory(UUT/USB_FunctionBlocks)
add_subdirectory(UUT/CDC_FunctionBlocks)

set(NIMIDI2_CUSTOM_SYSEX_DATA_ALLOCATOR ON CACHE BOOL "Build with custom sysex data allocator" FORCE)
add_subdirectory(lib/ni-midi2)
add_subdirectory(UUT_FreeRTOS/USB_MIDI_Echo)
add_subdirectory(UUT_FreeRTOS/FreeRTOS_Tasks)
