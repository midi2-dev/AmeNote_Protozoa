//
// Created by andrew on 19/05/22.
//

#include "pico/unique_id.h"
#include "pico/stdio/driver.h"
#include "include/interchip.h"
#include "include/umpProcessor.h"
#include "include/umpMessageCreate.h"
#include "include/cobs.h"

// for USB MIDI interface
#include "tusb.h"

#define DEVICE_MFRID 0x7D,0x00,0x00
#define DEVICE_FAMID 0x00,0x00
#define DEVICE_MODELID 0x00,0x00
#define DEVICE_VERSIONID 36,1,0,0
#define DEVICE_MIDIENPOINTNAME "AmeNote ProtoZOA"

interchip mainPico;
cobsUMP uutUMP;

umpProcessor MIDI2;

uint8_t fbStartGroup[3]={0,1,2};
uint32_t requestedBaudRate;

void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter);
void functionblock(uint8_t fbIdx, uint8_t filter);
void unknownUMPHandler(uint32_t *ump, uint8_t length);


void buttonDown(uint8_t button);
void buttonUp(uint8_t button);
void encoder(int dir);
void analog(uint8_t pot, uint16_t value);
void sendBufferUMP(uint8_t* buffer, uint8_t length);
void sendUMP(uint32_t* ump, int length);

int main() {
    stdio_init_all();
    stdio_set_driver_enabled(&stdio_usb, false);

    printf("Starting AmeNote ProtoZOA\n");

    mainPico.startup();
    mainPico.setButtonDown(buttonDown);
    mainPico.setButtonUp(buttonUp);
    mainPico.setAnalog(analog);
    mainPico.setEncoder(encoder);

    //Setup processing of UMP data recv to be handled by the following functions
    MIDI2.setMidiEndpoint(midiendpoint);
    MIDI2.setFunctionBlock(functionblock);
    MIDI2.setUnknownUMP(unknownUMPHandler);

    // Setup for TinyUSB
    tusb_init();



// ------- Loop Process incoming Messages
    while (true) {
        // Execute USB
       // tud_task();

        //read SPI from Main
        mainPico.process();

        //Read USB CDC
        char uBuf[1];
        int read = stdio_usb.in_chars(uBuf, 1);
        if (read > 0) {
            uutUMP.processSerial((uint8_t) uBuf[0]);
            if (uutUMP.availableUMP()) {
                uint32_t ump = uutUMP.readUMP();
                MIDI2.processUMP(ump);
            }
        }
        //taskYIELD();
    }
    return 0;
}

void unknownUMPHandler(uint32_t *umpMess, uint8_t length){
    uint8_t mt = (umpMess[0] >> 28)  & 0xF;
    if(mt!=0xf)return; //Message Type F
    uint16_t status = (umpMess[0]>> 16) & 0x3FF; //10 bit Status
    switch(status){
        case 0x030: { //Baud Rate Inquiry
            //Reply with 0x031
            //umpSerial.midiOutFunc('umpSerial',[((0xF << 28) >>> 0) + (0x031<<16), 0b11111111111,0,0]);
            uint32_t umpReply[4] = {(0xF << 28) + (0x031 << 16), 0b11111111111, 0, 0};
            sendUMP(umpReply, 4);
            break;
        }
        case 0x031: { //Reply to Baud Rate Inquiry
            //Add to Information and display
            break;
        }
        case 0x032: { //Baud Rate Request
            //Make Change
            requestedBaudRate = umpMess[1];
            //wait xxx
            sleep_ms(2000);
            uint32_t umpReply[4] = {(0xF << 28) + (0x033 << 16), 0x00AA55FF, 0xDEADBEEF, 0x4D494449};
            sendUMP(umpReply, 4);
            //Reply with 0x33
            break;
        }
        case 0x033: { //Baud Rate Request Reply Test 1
            //Reply with 0x34
            break;
        }
        case 0x034: { //Baud Rate Request Test 2
            //Reply with 0x35
            if(umpMess[1]==0x00AA55FF && umpMess[2]==0xDEADBEEF && umpMess[3]==0x4D494449 ) {
                uint32_t umpReply[4] = {(0xF << 28) + (0x035 << 16), requestedBaudRate, 0, 0};
                sendUMP(umpReply, 4);
            }
            break;
        }
        case 0x035: { //Baud Rate Ready
            //Continue Processing - refresh?
            break;
        }
    }




}

void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter){

    if(filter & 0x1){
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointInfoNotify(
                3, false, true, false, false);
        sendUMP(UMP.data(),4);
    }

    if(filter & 0x2){
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
                {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        sendUMP(UMP.data(),4);

    }

    if(filter & 0x3) {
        int friendlyNameLength = strlen(DEVICE_MIDIENPOINTNAME);
        for(uint8_t offset=0; offset<friendlyNameLength; offset+=14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENPOINTNAME,
                    friendlyNameLength);
            sendUMP(UMP.data(),4);
        }
    }
    if(filter & 0x4) {
        int len = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
        uint8_t buff[len];
        pico_get_unique_board_id_string((char *)buff, len);

        for(uint8_t offset=0; offset<len; offset+=14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_PRODID_NOTIFICATION, offset, (uint8_t *) buff,len);
            sendUMP(UMP.data(),4);
        }
    }
}

void functionblock(uint8_t fbIdx, uint8_t filter){

    if(fbIdx > 2 && fbIdx!=0xFF) return;


    if(filter & 0x1){
        if(fbIdx==0 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    1, true, 3, false, true, fbStartGroup[0], 2, 0x00,
                    0,0);
            sendUMP(UMP.data(),4);
        }
        if(fbIdx==1 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            sendUMP(UMP.data(),4);
        }
        if(fbIdx==2 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    3, true, 3, false, true, fbStartGroup[2], 1,  0x00,
                    1,0);
            sendUMP(UMP.data(),4);
        }
    }

    if(filter & 0x2) {
        if(fbIdx==0 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    1, 0, (uint8_t *) "FB 1", 4);
            sendUMP(UMP.data(),4);
        }
        if(fbIdx==1 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    2, 0, (uint8_t *) "FB 2", 4);
            sendUMP(UMP.data(),4);
        }
        if(fbIdx==2 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    3, 0, (uint8_t *) "FB 3", 4);
            sendUMP(UMP.data(),4);
        }
    }
}

void buttonDown(uint8_t button) {
    printf("Got Button Down %d \n", button);
    switch (button) {
        case CAP1: {
            if (fbStartGroup[0] > 0) { fbStartGroup[0]--; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    1, true, 3, false, true, fbStartGroup[0], 2, 0x00,
                    0, 0);
            sendUMP(UMP.data(), 4);
            break;
        }
        case CAP2:{
            if (fbStartGroup[0] <14) { fbStartGroup[0]++; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    1, true, 3, false, true, fbStartGroup[0], 2, 0x00,
                    0, 0);
            sendUMP(UMP.data(), 4);
            break;
        }
        case CAP3:{
            if (fbStartGroup[1] > 0) { fbStartGroup[1]--; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            sendUMP(UMP.data(),4);
            break;
        }
        case CAP4:{
            if (fbStartGroup[1] < 15) { fbStartGroup[1]++; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            sendUMP(UMP.data(),4);
            break;
        }
        case CAP5:
        case CAP6:
        case CAPRATIO:
            uint32_t ump = UMPMessage::mt2NoteOn(0,0,60+button,100);
            sendUMP(&ump, 1);
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
            break;
        case CAP6:
        case CAPRATIO:
            uint32_t ump = UMPMessage::mt2NoteOff(0,0,60+button,100);
            sendUMP(&ump, 1);
            break;
    }
}

void encoder(int dir) {
    printf("encoder Dir %d \n", dir);
}

void analog(uint8_t pot, uint16_t value) {
    printf("Pot %d %d\n", pot, value);

    uint32_t ump = UMPMessage::mt2CC(0, 0, pot==POT1?7:11, value >> 5);
    sendUMP(&ump, 1);

}

void sendUMP(uint32_t* ump, int length){
    uint8_t sBytes[length*4];
    uint8_t buffer[(length*4) + 2];
    uint8_t sbPos=0;
    for(uint8_t j = 0;j<length;j++){
        for(uint8_t i = 4;i;i--){
            sBytes[sbPos++] = ump[j] >> (8*(i-1));
        }
    }
    uint8_t newLength = cobsUMP::encode(sBytes, length*4, buffer);
    sendBufferUMP(buffer, newLength);
}

void sendBufferUMP(uint8_t* buffer, uint8_t length)
{
    stdio_usb.out_chars((char*)buffer, length);
    char uBuf[1]={0x00};
    stdio_usb.out_chars(uBuf, 1);
    //stdio_usb.out_flush();
}
