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

# PROTOZOA (default): ProtoZOA UUT J13 expansion pins (see wiznet_port/w5x00_spi.h).
# W5500_EVB_PICO: WIZnet's own W5500-EVB-Pico eval board instead, to isolate a
# software bug from a ProtoZOA-specific wiring/hardware issue.
set(NM2_WIZNET_BOARD "PROTOZOA" CACHE STRING "Wiznet SPI pin set: PROTOZOA or W5500_EVB_PICO")
set_property(CACHE NM2_WIZNET_BOARD PROPERTY STRINGS PROTOZOA W5500_EVB_PICO)

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
if(NM2_WIZNET_BOARD STREQUAL "W5500_EVB_PICO")
    target_compile_definitions(NM2BRIDGE_WIZNET_PORT PUBLIC NM2_WIZNET_BOARD_W5500_EVB_PICO=1)
endif()
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
# config). pico_lwip is a build-global CMake target, so this line affects
# every target in the whole build that links pico_lwip, not just this UUT --
# today that's only this one, but if a second UUT ever also links pico_lwip
# with a *different* lwipopts.h, this would silently pick whichever
# CMakeLists.txt happened to run last. Guard against that with a loud
# configure-time error instead of a silent misconfiguration.
get_property(_pico_lwip_opts_owner GLOBAL PROPERTY PICO_LWIP_OPTS_DIR_OWNER)
if(_pico_lwip_opts_owner AND NOT _pico_lwip_opts_owner STREQUAL CMAKE_CURRENT_LIST_DIR)
    message(FATAL_ERROR
            "pico_lwip's INTERFACE include directories were already set by "
            "${_pico_lwip_opts_owner} (a different lwipopts.h). pico_lwip is "
            "a single build-global target and cannot serve two conflicting "
            "lwIP configs -- give one of these targets its own pico_lwip-alike "
            "target instead of sharing the SDK one.")
endif()
set_property(GLOBAL PROPERTY PICO_LWIP_OPTS_DIR_OWNER ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(pico_lwip INTERFACE ${WIZNET_PORT_DIR})

# ---------------------------------------------------------------------------
# NetworkMIDI2 (binary distribution submodule)
# ---------------------------------------------------------------------------
set(NM2_DIR ${CMAKE_CURRENT_LIST_DIR}/../../lib/NetworkMIDI2)

# Role, device name, and network settings are runtime-configurable via the
# boot-time serial setup menu (see bridge_config.h/console_menu.h, issue
# #17) and persisted to flash. NM2_BRIDGE_ROLE only seeds the first-boot
# default -- once a config has been saved to flash, that's what's used.
set(NM2_BRIDGE_ROLE "CLIENT" CACHE STRING "NetworkMIDI2 session first-boot default role: HOST or CLIENT")
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
        bridge_config.cpp
        console_menu.cpp
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
        NM2_HAVE_LWIP_MDNS=1
        )
if(NM2_BRIDGE_ROLE STREQUAL "HOST")
    target_compile_definitions(UUT_NetworkMIDI2_Bridge PRIVATE NM2_BRIDGE_DEFAULT_ROLE_HOST=1)
endif()

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
        hardware_flash
        libmidi2
        NM2BRIDGE_WIZNET_PORT
        pico_lwip
        pico_lwip_nosys
        pico_lwip_mdns
        ${NM2_DIR}/lib/pico/rp2040/libnetworkmidi2.a
        ${NM2_DIR}/lib/pico/rp2040/libnm2_transport_lwip.a
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_NetworkMIDI2_Bridge)
