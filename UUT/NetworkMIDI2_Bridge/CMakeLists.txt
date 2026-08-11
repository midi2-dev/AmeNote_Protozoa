#
# UUT_NetworkMIDI2_Bridge: bridges Network MIDI 2.0 (UDP, over a Wiznet
# W5500 Ethernet expansion board on the UUT J13 header) to USB MIDI 2.0
# (tusb_ump), mirroring UUT/DIN_Bridge's structure but with a NetworkMIDI2
# session in place of the DIN UART.
#

# ---------------------------------------------------------------------------
# Wiznet ioLibrary_Driver (from the lib/RP2040-HAT-LWIP-C submodule) --
# assumes a W5500 expansion board. If yours is a W5100S board instead,
# change WIZNET_CHIP below and the W5500/ subdirectory references.
# ---------------------------------------------------------------------------
set(WIZNET_CHIP W5500)
set(WIZNET_DIR ${CMAKE_CURRENT_LIST_DIR}/../../lib/RP2040-HAT-LWIP-C/libraries/ioLibrary_Driver)
set(WIZNET_PORT_DIR ${CMAKE_CURRENT_LIST_DIR}/wiznet_port)

add_library(NM2BRIDGE_WIZNET_ETHERNET STATIC)
target_sources(NM2BRIDGE_WIZNET_ETHERNET PUBLIC
        ${WIZNET_DIR}/Ethernet/socket.c
        ${WIZNET_DIR}/Ethernet/wizchip_conf.c
        ${WIZNET_DIR}/Ethernet/W5500/w5500.c
        )
target_include_directories(NM2BRIDGE_WIZNET_ETHERNET PUBLIC
        ${WIZNET_DIR}/Ethernet
        ${WIZNET_DIR}/Ethernet/W5500
        )
target_compile_definitions(NM2BRIDGE_WIZNET_ETHERNET PUBLIC _WIZCHIP_=W5500)

# w5x00_spi.c / w5x00_lwip.c: the WIZnet SPI HAL + lwIP netif glue, vendored
# and customized for ProtoZOA's J13 pin-out -- see wiznet_port/w5x00_spi.h.
add_library(NM2BRIDGE_WIZNET_PORT STATIC)
target_sources(NM2BRIDGE_WIZNET_PORT PUBLIC
        ${WIZNET_PORT_DIR}/w5x00_spi.c
        ${WIZNET_PORT_DIR}/w5x00_lwip.c
        )
target_include_directories(NM2BRIDGE_WIZNET_PORT PUBLIC
        ${WIZNET_PORT_DIR}
        ${WIZNET_DIR}/Ethernet
        )
target_link_libraries(NM2BRIDGE_WIZNET_PORT PUBLIC
        NM2BRIDGE_WIZNET_ETHERNET
        pico_stdlib
        hardware_spi
        hardware_dma
        hardware_clocks
        pico_lwip
        pico_lwip_nosys
        )

# lwipopts.h must be visible to every pico_lwip consumer (WIZnet port code
# and NetworkMIDI2's LwipUdpTransport both build against the same lwIP
# config). No other target in this project uses pico_lwip, so extending its
# INTERFACE include path here is safe.
target_include_directories(pico_lwip INTERFACE ${WIZNET_PORT_DIR})

# ---------------------------------------------------------------------------
# NetworkMIDI2 (binary distribution submodule)
# ---------------------------------------------------------------------------
set(NM2_DIR ${CMAKE_CURRENT_LIST_DIR}/../../lib/NetworkMIDI2)

# HOST: listen for a NetworkMIDI2 client. CLIENT: connect out to a fixed
# host (see NM2_CLIENT_HOST_IP/NM2_CLIENT_HOST_PORT in main.cpp).
set(NM2_BRIDGE_ROLE "HOST" CACHE STRING "NetworkMIDI2 session role: HOST or CLIENT")
set_property(CACHE NM2_BRIDGE_ROLE PROPERTY STRINGS HOST CLIENT)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/*.cpp"
        "../../lib/tusb_ump/*.cpp"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_NetworkMIDI2_Bridge
        main.cpp
        usb_descriptors.cpp
        ${common_SRC}
        )

target_include_directories(UUT_NetworkMIDI2_Bridge PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../Common/include
        ../../lib/tusb_ump
        ../../lib/AM_MIDI2.0Lib
        ${NM2_DIR}/include
        ${NM2_DIR}/transports/lwip
        )

target_compile_definitions(UUT_NetworkMIDI2_Bridge PRIVATE
        NM2_BRIDGE_ROLE_${NM2_BRIDGE_ROLE}=1
        )

# Temporary suppression of compile warning for MIDI2_CPP (matches DIN_Bridge)
target_compile_options(UUT_NetworkMIDI2_Bridge PRIVATE -Wno-narrowing -Wno-unused-variable)

target_link_libraries(UUT_NetworkMIDI2_Bridge
        pico_stdlib
        pico_stdio
        pico_unique_id
        tinyusb_device
        hardware_spi
        hardware_dma
        hardware_clocks
        libmidi2
        NM2BRIDGE_WIZNET_PORT
        pico_lwip
        pico_lwip_nosys
        ${NM2_DIR}/lib/pico/rp2040/libnetworkmidi2.a
        ${NM2_DIR}/lib/pico/rp2040/libnm2_transport_lwip.a
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_NetworkMIDI2_Bridge)
