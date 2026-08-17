//
// Created by andrew on 19/05/22.
//



#include "pico/stdio/driver.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "hardware/pio.h"
#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

// for USB MIDI interface
#include "tusb.h"
#include "ump_device.h"

#include "include/bytestreamToUMP.h"
#include "include/umpToBytestream.h"
#include "include/umpProcessor.h"
#include "include/umpMessageCreate.h"

#define MIDI1_BAUD_RATE 31250

// Minimal UMP Endpoint Discovery identity. AmeNote does not have a registered
// SysEx manufacturer ID, so this uses the reserved "educational/non-commercial"
// prefix (0x7D) per the MIDI Association spec, matching UUT/USB_FunctionBlocks.
#define DEVICE_MFRID 0x7D,0x00,0x00
#define DEVICE_FAMID 0x00,0x00
#define DEVICE_MODELID 0x00,0x00
#define DEVICE_VERSIONID 0,1,0,0
#define DEVICE_MIDIENDPOINTNAME "AmeNote ProtoZOA DIN Bridge"

PIO pio = pio0 ;
uint smRx = 0;
uint smTx = 1;
void pio_rx_init(PIO pio, uint smrx);
void pio_tx_init(PIO pio, uint smtx);

bytestreamToUMP DIN2UMP;
umpToBytestream UMPConvert;
umpProcessor UMPHandler;

void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter);
void functionblock(uint8_t fbIdx, uint8_t filter);

// Diagnostic: log every alt-setting change so it can be correlated against
// the SET_INTERFACE control transfer in a USB trace (e.g. Beagle).
void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    printf("[%llu] ALT_SET itf=%d alt=%d (mVersion=%d)\n", time_us_64(), itf, alt, alt + 1);
}

// Reply to a host's UMP Endpoint Discovery request (Stream message, MT=0xF).
// Without this, a UMP-native host may never finalize this endpoint as a
// live MIDI source, even though raw UMP data is otherwise flowing correctly.
void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter) {
    (void) majVer;
    (void) minVer;

    if (filter & 0x1) {
        // 1 function block, MIDI 2.0 and MIDI 1.0 both supported, no JR timestamps.
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointInfoNotify(
                1, true, true, false, false);
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x2) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
                {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x4) {
        int nameLength = strlen(DEVICE_MIDIENDPOINTNAME);
        for (uint8_t offset = 0; offset < nameLength; offset += 14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENDPOINTNAME, nameLength);
            tud_ump_write_hton(0, UMP.data(), 4);
        }
    }
}

// Reply to a host's UMP Function Block Discovery request. DIN_Bridge exposes
// a single bidirectional function block covering UMP Group 1 (the one group
// actually bridged to/from the DIN port -- see DIN2UMP.defaultGroup below).
void functionblock(uint8_t fbIdx, uint8_t filter) {
    if (fbIdx != 0 && fbIdx != 0xFF) return;

    if (filter & 0x1) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                0, true, 3 /*bidirectional*/, false /*sender*/, false /*recv*/,
                0 /*firstGroup*/, 1 /*groupLength*/, 0x00 /*midiCISupport*/,
                3 /*isMIDI1: bandwidth restricted to 31.25 kbaud, matching DIN*/,
                0 /*maxS8Streams*/);
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x2) {
        char const *name = "DIN Bridge";
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                0, 0, (uint8_t *) name, strlen(name));
        tud_ump_write_hton(0, UMP.data(), 4);
    }
}

//int log( const char *format, ...);
int main() {
    stdio_init_all();

    printf("Starting AmeNote ProtoZOA\n");

    //---------- Setup MIDI Din Ports
    // Setup pio for receive
    pio_rx_init(pio, smRx);
    // Setup pio for transmit
    pio_tx_init(pio, smTx);

    //Set the UMP group of the output UMP message. By default, this is set to Group 1. Value is 0 based
    DIN2UMP.defaultGroup = 0;

    // Respond to the host's UMP Endpoint/Function Block Discovery requests so
    // it recognizes this device as a live MIDI 2.0 UMP endpoint.
    UMPHandler.setMidiEndpoint(midiendpoint);
    UMPHandler.setFunctionBlock(functionblock);

    // Setup for TinyUSB
    tusb_init();

// ------- Loop Process incoming Messages
    while (true) {
        // Execute USB
        tud_task();

        //Read USB MIDI
        if (tud_ump_n_mounted(0)) {
            uint32_t ump_n_available = tud_ump_n_available(0);
            uint32_t UMPpacket[4];
            uint32_t umpCount;
            if (ump_n_available) {
                uint8_t mVersion = tud_alt_setting(0) +1 ;
                if ((umpCount = tud_ump_read_ntoh(0, UMPpacket, 4))) {
                    switch (umpCount) {
                        case 1:
                            printf("[%llu] OUT MIDI %d UMP 0x%08x \n", time_us_64(), mVersion, UMPpacket[0]);
                            break;
                        case 2:
                            printf("[%llu] OUT MIDI %d UMP 0x%08x 0x%08x \n", time_us_64(), mVersion, UMPpacket[0], UMPpacket[1]);
                            break;
                        case 3:
                            printf("[%llu] OUT MIDI %d UMP 0x%08x 0x%08x 0x%08x\n", time_us_64(), mVersion, UMPpacket[0], UMPpacket[1], UMPpacket[2]);
                            break;
                        case 4:
                            printf("[%llu] OUT MIDI %d UMP 0x%08x 0x%08x 0x%08x 0x%08x\n", time_us_64(), mVersion, UMPpacket[0], UMPpacket[1], UMPpacket[2], UMPpacket[3]);
                            break;
                    }

                    for(uint8_t i=0; i < umpCount; i++) {
                        // Endpoint/Function Block Discovery and other UMP
                        // Stream messages are handled here (see midiendpoint()/
                        // functionblock() above); Channel Voice etc. pass
                        // through untouched since no callback is registered
                        // for them.
                        UMPHandler.processUMP(UMPpacket[i]);

                        UMPConvert.UMPStreamParse(UMPpacket[i]);
                        while (UMPConvert.availableBS()) {
                            uint8_t byte = UMPConvert.readBS();
                            if (UMPConvert.group == DIN2UMP.defaultGroup) {
                                uart_tx_program_putc(pio, smTx, byte);
                            }
                        }
                    }
                }
            }
            //--- End loop
        }


        //Read from DIN Port
        //-------------------
        if ( !pio_sm_is_rx_fifo_empty(pio, smRx) ) {
            // Get a character from the buffer
            uint8_t ch = uart_rx_program_getc(pio, smRx);
            if(ch == 0xFE) continue; //Skip ActiveSense
            DIN2UMP.bytestreamParse(ch);
            while(DIN2UMP.availableUMP()){
                uint32_t ump = DIN2UMP.readUMP();
                uint8_t mVersion = tud_alt_setting(0) + 1;
                printf("[%llu] IN  MIDI %d UMP 0x%08x\n", time_us_64(), mVersion, ump);
                tud_ump_write_hton(0,&ump,1);
            }
        }
    }
    return 0;
}



void pio_rx_init(PIO piorx, uint smrx) {
    // Set up the state machine we're going to use to receive them.
    uint offset = pio_add_program(piorx, &uart_rx_program);
    uart_rx_program_init(piorx, smrx, offset, 13, MIDI1_BAUD_RATE);
}
void pio_tx_init(PIO piotx, uint smtx) {
    uint offset = pio_add_program(piotx, &uart_tx_program);
    uart_tx_program_init(piotx, smtx, offset, 12, MIDI1_BAUD_RATE);
}





