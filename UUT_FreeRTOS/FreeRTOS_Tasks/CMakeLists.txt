# project options
option(PROTOZOA_EXPANSION_CME_WIDI_CORE "Enable WIDI Core expansion module" OFF)
option(PROTOZOA_EXPANSION_SERIAL_TYPE25 "Enable Type 25 expansion module"   OFF)
option(PROTOZOA_EXPANSION_ETHERNET_W5500 "Enable ethernet expansion module" OFF)

option(PROTOZOA_USB_CDC_SERIAL "Enable USB CDC serial transport" ON)

option(PROTOZOA_SERIAL_BRACKET16 "Enable 16 Bit Bracketing for serial UMP connections" OFF)

option(PROTOZOA_TRACE_INCOMING_TRAFFIC "Trace incoming traffic on serial console" OFF)
option(PROTOZOA_TRACE_OUTGOING_TRAFFIC "Trace outgoing traffic on serial console" OFF)

file(GLOB common_SRC CONFIGURE_DEPENDS
        "../../Common/include *.h"
        "../../Common/*.c"
        "../../Common/*.cpp"
        "../../lib/tusb_ump/*.h"
        "../../lib/tusb_ump/*.c"
        "../../lib/tusb_ump/*.cpp"
        "../../lib/AM_MIDI2.0Lib/include/*.h"
        "../../lib/AM_MIDI2.0Lib/src/*.cpp"
        )

add_executable(UUT_FREERTOS_TASKS
        BlinkTask.cpp
        DINSerialTask.cpp
        PicoMainTask.cpp
        PEHeaderParser.cpp
        UMPProcessing.cpp
        USBMIDITask.cpp
        main.c
        ${common_SRC}
        ${usb_SRC}
        )

set(numExpansionsEnabled 0)
if (PROTOZOA_EXPANSION_CME_WIDI_CORE)
        message(DEBUG "Enabling CME WIDI expansion.")
        math(EXPR numExpansionsEnabled "${numExpansionsEnabled}+1")
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_EXPANSION_CME_WIDI_CORE=1)
        target_sources(UUT_FREERTOS_TASKS PRIVATE CMEWidiTask.cpp usb_descriptors.bt.cpp)
        set_target_properties(UUT_FREERTOS_TASKS PROPERTIES OUTPUT_NAME UUT_FREERTOS_TASKS_WIDI)
else()
        target_sources(UUT_FREERTOS_TASKS PRIVATE usb_descriptors.3groups.cpp)
endif()
if (PROTOZOA_EXPANSION_SERIAL_TYPE25)
        message(DEBUG "Enabling Type25 expansion.")
        math(EXPR numExpansionsEnabled "${numExpansionsEnabled}+1")
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_EXPANSION_SERIAL_TYPE25=1)
        target_sources(UUT_FREERTOS_TASKS PRIVATE Type25SerialTask.cpp)
        if(PROTOZOA_SERIAL_BRACKET16)
                set_target_properties(UUT_FREERTOS_TASKS PROPERTIES OUTPUT_NAME UUT_FREERTOS_TASKS_Type25_Bracket16)
        else()
                set_target_properties(UUT_FREERTOS_TASKS PROPERTIES OUTPUT_NAME UUT_FREERTOS_TASKS_Type25_COBS)
        endif()
endif()
if (PROTOZOA_EXPANSION_ETHERNET_W5500)
        message(DEBUG "Enabling Ethernet W5500 expansion.")
        math(EXPR numExpansionsEnabled "${numExpansionsEnabled}+1")
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_EXPANSION_ETHERNET_W5500=1)
        target_sources(UUT_FREERTOS_TASKS PRIVATE EthernetW5500Task.cpp)
        set_target_properties(UUT_FREERTOS_TASKS PROPERTIES OUTPUT_NAME UUT_FREERTOS_TASKS_Ethernet)
endif()
if (numExpansionsEnabled GREATER 1)
        message(FATAL_ERROR "ProtoZOA extension slot can only hold one board at a time!")
endif()

if (PROTOZOA_USB_CDC_SERIAL)
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_USB_CDC_SERIAL=1)
        target_sources(UUT_FREERTOS_TASKS PRIVATE USBCDCSerialTask.cpp)
endif()
    
if (PROTOZOA_SERIAL_BRACKET16)
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_SERIAL_BRACKET16=1)
endif()

if (PROTOZOA_TRACE_INCOMING_TRAFFIC)
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_TRACE_INCOMING_TRAFFIC=1)
endif()

if (PROTOZOA_TRACE_OUTGOING_TRAFFIC)
        target_compile_definitions(UUT_FREERTOS_TASKS PRIVATE PROTOZOA_TRACE_OUTGOING_TRAFFIC=1)
endif()

# Make sure TinyUSB can find tusb_config.h
target_include_directories(UUT_FREERTOS_TASKS PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        .
        ../../Common
        ../../Common/include
        ../../lib/tusb_ump
        )

pico_generate_pio_header(UUT_FREERTOS_TASKS ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_rx.pio)
pico_generate_pio_header(UUT_FREERTOS_TASKS ${CMAKE_CURRENT_LIST_DIR}/../../Common/pio_serial/uart_tx.pio)

#Temporary suppression of compile waring for MIDI2_CPP
target_compile_options(UUT_FREERTOS_TASKS PRIVATE -Wno-narrowing -Wno-unused-variable)

# pull in common dependencies
target_link_libraries(UUT_FREERTOS_TASKS
        pico_stdlib
        pico_stdio
        pico_unique_id
        tinyusb_device
        hardware_pio
        hardware_adc
        hardware_spi
        hardware_flash
        hardware_dma
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap1
        ni-midi2
)

# pico_enable_stdio_usb(ProtoZOA_UUT 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(UUT_FREERTOS_TASKS)
