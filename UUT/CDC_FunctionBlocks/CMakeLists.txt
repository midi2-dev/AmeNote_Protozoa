
file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/*.cpp"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_CDC_FB
        main.cpp
        ${common_SRC}
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(UUT_CDC_FB PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../Common/include
        ../../lib/AM_MIDI2.0Lib
        )

#pico_generate_pio_header(UUT_USB_FB ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_rx.pio)
#pico_generate_pio_header(UUT_USB_FB ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_tx.pio)

#Temporary suppression of compile waring for MIDI2_CPP
target_compile_options(UUT_CDC_FB PRIVATE -Wno-narrowing -Wno-unused-variable)

# pull in common dependencies
target_link_libraries(UUT_CDC_FB
        pico_stdlib pico_stdio hardware_pio hardware_adc hardware_spi
)

pico_enable_stdio_usb(UUT_CDC_FB 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_CDC_FB)
