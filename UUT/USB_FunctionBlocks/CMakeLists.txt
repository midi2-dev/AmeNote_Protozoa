
file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/*.cpp"
        "../../lib/tusb_ump/*.cpp"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_USB_FB
        main.cpp
        usb_descriptors.cpp
        ${common_SRC}
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(UUT_USB_FB PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../Common/include
        ../../lib/tusb_ump
        ../../lib/AM_MIDI2.0Lib
        )

#pico_generate_pio_header(UUT_USB_FB ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_rx.pio)
#pico_generate_pio_header(UUT_USB_FB ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_tx.pio)

#Temporary suppression of compile waring for MIDI2_CPP
target_compile_options(UUT_USB_FB PRIVATE -Wno-narrowing -Wno-unused-variable)

# pull in common dependencies
target_link_libraries(UUT_USB_FB
        pico_stdlib
        pico_stdio
        pico_unique_id
        tinyusb_device
        hardware_pio
        hardware_adc
        hardware_spi
        hardware_flash
        hardware_dma
        midi2
)



# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_USB_FB)
