//
// Created by andrew on 24/05/22.
//

#include "include/cobs.h"


void cobsUMP::processSerial(uint8_t serialByte){
    //Process COBS
    if(serialByte == 0){
        uintBlockLength = 0;
        usPos = 4;
        ump = 0;
        return;
    }
    if(!uintBlockLength){
        uintBlockLength = serialByte;
        serialByte=0;
    }
    if(usPos!=4)ump += serialByte << (8 * usPos);
    if(!usPos){
        umpAvail=true;
        usPos = 3;

    }else{
        usPos--;
    }
    uintBlockLength--;

}

bool cobsUMP::availableUMP() {
    return umpAvail;
}

uint32_t cobsUMP::readUMP() {
    uint32_t outUMP = ump;
    umpAvail = false;
    ump = 0;
    return outUMP;
}


uint8_t cobsUMP::encode(const void *data, uint8_t length, uint8_t *buffer)
{

    uint8_t *encode = buffer; // Encoded byte pointer
    uint8_t *codep = encode++; // Output code pointer
    uint8_t code = 1; // Code value

    for (const auto *byte = (const uint8_t *)data; length--; ++byte)
    {
        if (*byte) // Byte not zero, write it
            *encode++ = *byte, ++code;

        if (!*byte || code == 0xff) // Input is zero or block completed, restart
        {
            *codep = code, code = 1, codep = encode;
            if (!*byte || length)
                ++encode;
        }
    }
    *codep = code; // Write final code value

    return (uint8_t)(encode - buffer);
}


//void cobsUMP::sendBufferUMP(uint8_t* buffer, uint8_t length)
//{
//    if (tud_cdc_connected())
//    {
//        for (uint i = 0; i < length; i++)
//            tud_cdc_write_char(buffer[i]);
//
//        tud_cdc_write_char(0x00);
//        tud_cdc_write_flush();
//    }
//}


//void cobsUMP::sendUMP(uint32_t * ump, uint8_t size){
//    uint8_t sBytes[size*4];
//    uint8_t buffer[size*4 + 2];
//    uint8_t sbPos=0;
//    for(uint8_t j = 0;j<size;j++){
//        for(uint8_t i = 4;i;i--){
//            sBytes[sbPos++] = ump[j] >> (8*(i-1));
//        }
//    }
//    uint8_t newLength = encode(sBytes, size*4, buffer);
//    //sendBufferUMP(buffer, newLength);
//}
