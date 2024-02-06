//
// Created by andrew on 19/05/22.
//

#include "pico/stdio/driver.h"
#include "hardware/uart.h"
#include "hardware/pio.h"

// for USB MIDI interface
#include "tusb.h"
#include "ump_device.h"

#include "include/bytestreamToUMP.h"
#include "include/umpToBytestream.h"

//--- CME Defines do not change
#define CME_ON_PIN 10
#define CME_BUTTON_PIN 11
#define CME_STATUS_1_PIN 16
#define CME_STATUS_2_PIN 15
#define CME_HSDISABLE_PIN 2
#define CME_RESET_PIN 3

#define CME_UART            uart1
#define CME_UART_TX         8 // GPIO8
#define CME_UART_RX         9 // GPIO9

#define MIDI1_BAUD_RATE 31250
#define CMEFULL_BAUD_RATE 400000

bytestreamToUMP CME2UMP;
umpToBytestream UMPConvert;

void setupCME(bool);



//int log( const char *format, ...);

int main()
{
    // Initialize for output of UART0 as stdout

    stdio_init_all();

    printf("Starting AmeNote ProtoZOA\n");

    setupCME(false); //Set to true to have Full Rate

    //Set the UMP group of the output UMP message. By default, this is set to Group 1. Value is 0 based
    CME2UMP.defaultGroup = 0;

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
                            if (UMPConvert.group == CME2UMP.defaultGroup) {
                                uart_putc_raw(CME_UART, byte);
                            }
                        }
                    }
                }
            }


            //--- End loop
        }


        //Read Expansion Port
        //-------------------
        if (uart_is_readable(CME_UART)) {
            uint8_t ch = uart_getc(CME_UART);
            if (ch == 0xFE) continue; //Skip ActiveSense
            CME2UMP.bytestreamParse(ch);
            while (CME2UMP.availableUMP()) {
                uint32_t ump = CME2UMP.readUMP();
                tud_ump_write(0, &ump, 1);
            }
        }

    }
    return 0;
}

//********************** UARTs etc
void setupCME(bool fullRate){
    // Setup CME IO Pins
    
    // Setup UART baud rate and also configur CME module for selected rate
    // must be done before CME module reset.
    gpio_init(CME_HSDISABLE_PIN);
    gpio_set_dir(CME_HSDISABLE_PIN, GPIO_OUT);
    if(fullRate) {
        uart_init(CME_UART, CMEFULL_BAUD_RATE);
        gpio_put(CME_HSDISABLE_PIN,false); // sets to normal speed of 400000
    }else{
        uart_init(CME_UART, MIDI1_BAUD_RATE);
        gpio_put(CME_HSDISABLE_PIN,true); // sets to normal speed of 31250
    }
    // Setup Pico pins for UART function
    gpio_set_function(CME_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(CME_UART_RX, GPIO_FUNC_UART);

    gpio_init(CME_ON_PIN);
    gpio_set_dir(CME_ON_PIN, GPIO_OUT);
    gpio_put(CME_ON_PIN, false);

    // Following used if want to force an operation status - we can use just default startup.
    // gpio_init(CME_BUTTON_PIN);
    // gpio_set_dir(CME_BUTTON_PIN, GPIO_OUT);
    //gpio_put(CME_BUTTON_PIN, true);
    
    gpio_init(CME_STATUS_1_PIN);
    gpio_set_dir(CME_STATUS_1_PIN, GPIO_IN);
    gpio_init(CME_STATUS_2_PIN);
    gpio_set_dir(CME_STATUS_2_PIN, GPIO_IN);

    gpio_init(CME_RESET_PIN);
    gpio_set_dir(CME_RESET_PIN, GPIO_OUT);
    gpio_put(CME_RESET_PIN, true); // to not want in reset


    // bring up the CME module
    // powerup
    gpio_put(CME_ON_PIN, true);

}




