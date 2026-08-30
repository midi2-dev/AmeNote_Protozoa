
file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/*.cpp"
        "../../lib/tusb_ump/*.cpp"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_UART_DIN_BRIDGE
        main.cpp
        usb_descriptors.cpp
        uart_din_bridge.cpp
        ${common_SRC}
        )

# List this example's own directory FIRST so its local tusb_config.h
# (CFG_TUD_UMP=1) is found ahead of Common/include's shared one.
target_include_directories(UUT_UART_DIN_BRIDGE PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../lib/tusb_ump
        ../../lib/AM_MIDI2.0Lib
        )

#Temporary suppression of compile waring for MIDI2_CPP
target_compile_options(UUT_UART_DIN_BRIDGE PRIVATE -Wno-narrowing -Wno-unused-variable)

# pull in common dependencies
target_link_libraries(UUT_UART_DIN_BRIDGE
        pico_stdlib
        pico_stdio
        pico_unique_id
        tinyusb_device
        hardware_uart
        hardware_adc
        hardware_spi
        hardware_flash
        hardware_dma
        libmidi2
)

# This example's DIN bridge drives GPIO12/13 -- the RP2040's only UART0-
# capable pins matching ProtoZOA's DIN header. stdio_init_all()'s default
# console claims GP0/1 as that same UART0's TX/RX, and confirmed by testing
# on real hardware, that claim blocks UART0's RX routing from GPIO13
# entirely (not just a baud-rate clash on TX -- GPIO13 saw zero bytes ever,
# regardless of pull-ups, until stdio-over-UART was disabled). So stdio-
# over-UART must stay off here; there's no live console for this example.
# See README.md for the full story.
pico_enable_stdio_uart(UUT_UART_DIN_BRIDGE 0)
pico_enable_stdio_usb(UUT_UART_DIN_BRIDGE 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_UART_DIN_BRIDGE)
