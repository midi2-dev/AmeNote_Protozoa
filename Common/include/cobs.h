//
// Created by andrew on 24/05/22.
//

#ifndef COBS_H
#define COBS_H
#include <cstdint>

class cobsUMP {
private:
    uint8_t usPos = 4;
    uint8_t uintBlockLength = 0;
    uint32_t ump;
    bool umpAvail = false;
    //void sendBufferUMP(uint8_t* buffer, uint8_t length);

public:

    void processSerial(uint8_t serialByte);
    bool availableUMP();
    uint32_t readUMP();

    //void sendUMP(uint32_t * ump, uint8_t size);
    static uint8_t encode(const void *data, uint8_t length, uint8_t *buffer);

};




#endif //PICO_EXAMPLES_COBS_H
