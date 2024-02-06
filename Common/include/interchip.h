//
// Created by andrew on 27/05/22.
//

#ifndef PICO_EXAMPLES_INTERCHIP_H
#define PICO_EXAMPLES_INTERCHIP_H

#define CAP1 0x0
#define CAP2 0x1
#define CAP3 0x2
#define CAP4 0x3
#define CAP5 0x4
#define CAP6 0x5
#define CAPRATIO 0x6

#define DISP_UP 0x7
#define DISP_DOWN 0x8
#define DISP_LEFT 0x9
#define DISP_RIGHT 0xA
#define DISP_CENTER 0xB
#define DISP_A 0xC
#define DISP_B 0xD

#define POT1 0x0
#define POT2 0x1

#define BUTTON_UP 0x80
#define BUTTON_DOWN 0x90

#define POTS 0xA0
#define ENCODER 0xB0

#include "pico/stdlib.h"

class interchip {
private:
    uint8_t in_buf[1];

    uint8_t messType;
    uint8_t potType;
    int analogMSB = -1;

    void (*buttonup)(uint8_t button) = nullptr;
    void (*buttondown)(uint8_t button) = nullptr;
    void (*analog)(uint8_t pot, uint16_t value) = nullptr;
    void (*encoder)(int dir) = nullptr;

public:

    void startup();

    void process();

    inline void setButtonUp(void (*fptr)(uint8_t button)){ buttonup = fptr; }
    inline void setButtonDown(void (*fptr)(uint8_t button)){ buttondown = fptr; }
    inline void setAnalog(void (*fptr)(uint8_t pot, uint16_t value)){ analog = fptr; }
    inline void setEncoder(void (*fptr)(int dir)){ encoder = fptr; }
};


#endif //PICO_EXAMPLES_INTERCHIP_H
