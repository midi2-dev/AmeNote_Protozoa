//
// Created by andrew on 19/05/22.
//



#include "pico/stdio/driver.h"
#include "hardware/pio.h"
#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

// for USB MIDI interface
#include "tusb.h"
#include "ump_device.h"

#include "include/bytestreamToUMP.h"
#include "include/umpToBytestream.h"

#define MIDI1_BAUD_RATE 31250

PIO pio = pio0 ;
uint smRx = 0;
uint smTx = 1;
void pio_rx_init(PIO pio, uint smrx);
void pio_tx_init(PIO pio, uint smtx);

bytestreamToUMP DIN2UMP;
umpToBytestream UMPConvert;



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
                if ((umpCount = tud_ump_read(0, UMPpacket, 4))) {
                    switch (umpCount) {
                        case 1:
                            printf("MIDI %d UMP 0x%08x \n", mVersion, UMPpacket[0]);
                            break;
                        case 2:
                            printf("MIDI %d UMP 0x%08x 0x%08x \n", mVersion, UMPpacket[0], UMPpacket[1]);
                            break;
                        case 3:
                            printf("MIDI %d UMP 0x%08x 0x%08x 0x%08x\n", mVersion, UMPpacket[0], UMPpacket[1], UMPpacket[2]);
                            break;
                        case 4:
                            printf("MIDI %d UMP 0x%08x 0x%08x 0x%08x 0x%08x\n", mVersion, UMPpacket[0], UMPpacket[1], UMPpacket[2], UMPpacket[3]);
                            break;
                    }

                    for(uint8_t i=0; i < umpCount; i++) {
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
                tud_ump_write(0,&ump,1);
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





