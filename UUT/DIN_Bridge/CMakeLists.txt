
file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/*.cpp"
        "../../lib/tusb_ump/*.cpp"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_DIN_BRIDGE
        main.cpp
        usb_descriptors.cpp
        ${common_SRC}
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(UUT_DIN_BRIDGE PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../Common/include
        ../../lib/tusb_ump
        ../../lib/AM_MIDI2.0Lib
        )

pico_generate_pio_header(UUT_DIN_BRIDGE ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_rx.pio)
pico_generate_pio_header(UUT_DIN_BRIDGE ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_tx.pio)

#Temporary suppression of compile waring for MIDI2_CPP
target_compile_options(UUT_DIN_BRIDGE PRIVATE -Wno-narrowing -Wno-unused-variable)

# pull in common dependencies
target_link_libraries(UUT_DIN_BRIDGE
        pico_stdlib
        pico_stdio
        pico_unique_id
        tinyusb_device
        hardware_pio
        hardware_adc
        hardware_spi
        hardware_flash
        hardware_dma
        libmidi2
)

# pico_enable_stdio_usb(ProtoZOA_UUT 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_DIN_BRIDGE)
