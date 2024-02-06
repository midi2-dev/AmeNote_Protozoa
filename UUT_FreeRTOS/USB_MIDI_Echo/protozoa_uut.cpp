//
// Created by andrew on 19/05/22.
//


#include "include/interchip.h"

#include "FreeRTOS.h"
#include "task.h"

// for USB MIDI interface
#include "ump_device.h"

interchip mainPico;


void buttonDown(uint8_t button);
void buttonUp(uint8_t button);
void encoder(int dir);
void analog(uint8_t pot, uint16_t value);

extern "C"
{
int ProtoZOA_main() {


    printf("Starting AmeNote ProtoZOA\n");

    mainPico.startup();
    mainPico.setButtonDown(buttonDown);
    mainPico.setButtonUp(buttonUp);
    mainPico.setAnalog(analog);
    mainPico.setEncoder(encoder);



// ------- Loop Process incoming Messages
    while (true) {
        //read SPI from Main
        mainPico.process();

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

                    tud_ump_write(0, UMPpacket, umpCount);

                }
            }


            taskYIELD();

            //--- End loop
        }
    }
    return 0;
}
}

void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    (void) itf;
    printf("UMP on USB enabled: %d \n", alt);
}

void buttonDown(uint8_t button) {
    printf("Got Button Down %d \n", button);
    switch (button) {
        case CAP1:
        case CAP2:
        case CAP3:
        case CAP4:
        case CAP5:
        case CAP6:
        case CAPRATIO:
            break;
    }
}

void buttonUp(uint8_t button) {
    printf("Got Button Up %d \n", button);
    switch (button) {
        case CAP1:
        case CAP2:
        case CAP3:
        case CAP4:
        case CAP5:
        case CAP6:
        case CAPRATIO:
            break;
    }
}

void encoder(int dir) {
    printf("encoder Dir %d \n", dir);
}

void analog(uint8_t pot, uint16_t value) {
    printf("Pot %d %d\n", pot, value);
}






