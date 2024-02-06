cmake_minimum_required(VERSION 3.12)

project(ProtoZOA_Main)

pico_sdk_init()

file(GLOB lcd_SRC CONFIGURE_DEPENDS "lcd/*.h" "lcd/*.c")
file(GLOB common_SRC CONFIGURE_DEPENDS "../Common/include *.h" "../Common/*.c")

add_executable(ProtoZOA_Main
        picoprobe/src/led.c
        #picoprobe/src/main.c
        picoprobe/src/usb_descriptors.c
        picoprobe/src/probe.c
        picoprobe/src/cdc_uart.c
        picoprobe/src/get_serial.c
        picoprobe/src/sw_dp_pio.c
        picoprobe/src/tusb_edpt_handler.c
        main.c
        protozoa_main.c
        #picoprobe/src/main.c
        ${lcd_SRC}
        ${common_SRC}
)

set(DBG_PIN_COUNT=4)

target_sources(ProtoZOA_Main PRIVATE
        ../lib/CMSIS_5/CMSIS/DAP/Firmware/Source/DAP.c
        ../lib/CMSIS_5/CMSIS/DAP/Firmware/Source/JTAG_DP.c
        ../lib/CMSIS_5/CMSIS/DAP/Firmware/Source/DAP_vendor.c
        ../lib/CMSIS_5/CMSIS/DAP/Firmware/Source/SWO.c
        #CMSIS_5/CMSIS/DAP/Firmware/Source/SW_DP.c
        )

target_include_directories(ProtoZOA_Main PRIVATE
        .
        ../lib/CMSIS_5/CMSIS/DAP/Firmware/Include/
        ../lib/CMSIS_5/CMSIS/Core/Include/
        picoprobe/include/
        picoprobe/src/
        ../Common
)

target_compile_options(ProtoZOA_Main PRIVATE -Wall)

pico_generate_pio_header(ProtoZOA_Main ${CMAKE_CURRENT_LIST_DIR}/picoprobe/src/probe.pio)
pico_generate_pio_header(ProtoZOA_Main ${CMAKE_CURRENT_LIST_DIR}/picoprobe/src/probe_oen.pio)

target_link_libraries(ProtoZOA_Main PRIVATE
        pico_stdlib
        pico_unique_id
        tinyusb_device
        tinyusb_board
        hardware_pio
        pico_multicore
        hardware_adc
        hardware_spi
        hardware_pwm
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap1
)

#pico_set_binary_type(ProtoZOA_Main copy_to_ram)

pico_add_extra_outputs(ProtoZOA_Main)
