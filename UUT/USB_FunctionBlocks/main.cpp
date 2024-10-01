//
// Created by andrew on 19/05/22.
//

#include "pico/unique_id.h"
#include "include/interchip.h"
#include "include/umpProcessor.h"
#include "include/umpMessageCreate.h"
#include "include/midiCIProcessor.h"

// for USB MIDI interface
#include "tusb.h"
#include "ump_device.h"
#include "include/midiCIMessageCreate.h"


#define DEVICE_MFRID 0x7D,0x00,0x00
#define DEVICE_FAMID 0x00,0x00
#define DEVICE_MODELID 0x00,0x00
#define DEVICE_VERSIONID 36,1,0,0
#define DEVICE_MIDIENPOINTNAME "AmeNote ProtoZOA"
#define MIDICI_MESSAGEFORMATVERSION 0x02

interchip mainPico;

umpProcessor UMPHandler;
midiCIProcessor MIDICIHandler;
uint32_t m2procMUID;
bool isProcMIDICI = false;

uint8_t fbStartGroup[3]={0,1,2};

void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter);
void functionblock(uint8_t fbIdx, uint8_t filter);
void processUMPSysex(struct umpData mess);
void sendOutSysex(uint8_t group, uint8_t *sysex ,uint16_t length, uint8_t state);

uint32_t random(uint32_t max);
bool checkMUIDCallback(uint8_t group, uint32_t muid);
void recvDiscovery(struct MIDICI ciDetails, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                   std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t remoteciSupport,
                   uint16_t remotemaxSysex, uint8_t outputPathId
);
void handleProfileInquiry(MIDICI ciDetails);
void handleProfileDetailsInquiry(MIDICI ciDetails, std::array<uint8_t, 5> profile,   uint8_t InquiryTarget);
void invalidMUID(uint8_t group, struct MIDICI ciDetails, uint32_t terminateMuid);


void buttonDown(uint8_t button);
void buttonUp(uint8_t button);
void encoder(int dir);
void analog(uint8_t pot, uint16_t value);

int main() {
    stdio_init_all();

    printf("Starting AmeNote ProtoZOA\n");

    mainPico.startup();
    mainPico.setButtonDown(buttonDown);
    mainPico.setButtonUp(buttonUp);
    mainPico.setAnalog(analog);
    mainPico.setEncoder(encoder);

    //Setup processing of UMP data recv to be handled by the following functions

    UMPHandler.setMidiEndpoint(midiendpoint);
    UMPHandler.setFunctionBlock(functionblock);
    UMPHandler.setSysEx(processUMPSysex);

    m2procMUID = random(0xFFFFEFF);
    MIDICIHandler.setCheckMUID(checkMUIDCallback);
    MIDICIHandler.setRecvDiscovery(recvDiscovery);
    MIDICIHandler.setRecvProfileInquiry(handleProfileInquiry);
    MIDICIHandler.setRecvProfileDetailsInquiry(handleProfileDetailsInquiry);

    // Setup for TinyUSB
    tusb_init();



// ------- Loop Process incoming Messages
    while (true) {
        // Execute USB
        tud_task();

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
                            printf("MIDI %d UMP 0x%08x 0x%08x 0x%08x\n", mVersion, UMPpacket[0], UMPpacket[1],
                                   UMPpacket[2]);
                            break;
                        case 4:
                            printf("MIDI %d UMP 0x%08x 0x%08x 0x%08x 0x%08x\n", mVersion, UMPpacket[0], UMPpacket[1],
                                   UMPpacket[2], UMPpacket[3]);
                            break;
                    }

                    for(uint32_t i=0; i< umpCount; i++){
                        UMPHandler.processUMP(UMPpacket[i]);
                    }

                }
            }
        }
    }
    return 0;
}

void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    (void) itf;
    printf("UMP on USB enabled: %d \n", alt);
}


void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter){

    if(filter & 0x1){
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointInfoNotify(
                3, false, true, false, false);
        tud_ump_write(0,UMP.data(),4);
    }

    if(filter & 0x2){
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
                {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        tud_ump_write(0,UMP.data(),4);

    }

    if(filter & 0x3) {
        int friendlyNameLength = strlen(DEVICE_MIDIENPOINTNAME);
        for(uint8_t offset=0; offset<friendlyNameLength; offset+=14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENPOINTNAME,
                    friendlyNameLength);
            tud_ump_write(0,UMP.data(),4);
        }
    }
    if(filter & 0x4) {
        int len = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
        uint8_t buff[len];
//        int len = 16;
//        uint8_t buff[] = "EFECE0EE4E911614";
        pico_get_unique_board_id_string((char *)buff, len);

        for(uint8_t offset=0; offset<len; offset+=14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_PRODID_NOTIFICATION, offset, (uint8_t *) buff,len);
            tud_ump_write(0,UMP.data(),4);
        }
    }
}

void functionblock(uint8_t fbIdx, uint8_t filter){

    if(fbIdx > 2 && fbIdx!=0xFF) return;


    if(filter & 0x1){
        if(fbIdx==0 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    0, true, 3, false, true, fbStartGroup[0], 2, 0x00,
                    0,0);
            tud_ump_write(0,UMP.data(),4);
        }
        if(fbIdx==1 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    1, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            tud_ump_write(0,UMP.data(),4);
        }
        if(fbIdx==2 || fbIdx==0xFF){
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true, fbStartGroup[2], 1,  0x00,
                    1,0);
            tud_ump_write(0,UMP.data(),4);
        }
    }

    if(filter & 0x2) {
        if(fbIdx==0 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    0, 0, (uint8_t *) "FB 1", 4);
            tud_ump_write(0,UMP.data(),4);
        }
        if(fbIdx==1 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    1, 0, (uint8_t *) "FB 2", 4);
            tud_ump_write(0,UMP.data(),4);
        }
        if(fbIdx==2 || fbIdx==0xFF) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                    2, 0, (uint8_t *) "FB 3", 4);
            tud_ump_write(0,UMP.data(),4);
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
            tud_ump_write(0, UMP.data(), 4);
            break;
        }
        case CAP2:{
            if (fbStartGroup[0] <14) { fbStartGroup[0]++; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    1, true, 3, false, true, fbStartGroup[0], 2, 0x00,
                    0, 0);
            tud_ump_write(0, UMP.data(), 4);
            break;
        }
        case CAP3:{
            if (fbStartGroup[1] > 0) { fbStartGroup[1]--; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            tud_ump_write(0,UMP.data(),4);
            break;
        }
        case CAP4:{
            if (fbStartGroup[1] < 15) { fbStartGroup[1]++; }
            std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                    2, true, 3, false, true,fbStartGroup[1], 1,  0x00,
                    1,0);
            tud_ump_write(0,UMP.data(),4);
            break;
        }
        case CAP5:
        case CAP6:
        case CAPRATIO:
            uint32_t ump = UMPMessage::mt2NoteOn(0,0,60+button,100);
            if (tud_ump_n_mounted(0))
            {
                tud_ump_write(0, &ump, 1);
            }
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
            if (tud_ump_n_mounted(0))
            {
                tud_ump_write(0, &ump, 1);
            }
            break;
    }
}

void encoder(int dir) {
    printf("encoder Dir %d \n", dir);
}

void analog(uint8_t pot, uint16_t value) {
    printf("Pot %d %d\n", pot, value);

    uint32_t ump = UMPMessage::mt2CC(0, 0, pot==POT1?7:11, value >> 5);
    if (tud_ump_n_mounted(0))
    {
        tud_ump_write(0, &ump, 1);
    }
}

uint8_t sxPos=0;
std::array<uint8_t, 6> sx;
void sendOutSysex(uint8_t group, uint8_t *sysex ,uint16_t length, uint8_t state){
    if (state < 2) {
        sxPos=0;
    }
    for (int i = 0; i < length; i++) {
        sx[sxPos++]=sysex[i] & 0x7F;
        if(sxPos == 6){
            std::array<uint32_t, 2> UMP = UMPMessage::mt3Sysex7(group, state < 2 && i < 6 ? 1 : 2, 6, sx);
            tud_ump_write(0,UMP.data(),2);
            sxPos=0;
        }
    }
    if (state == 0 || state == 3) {
//        uint32_t UMPF8 = UMPMessage::mt1TimingClock(group);
//        tud_ump_write(0,&UMPF8,1);
        std::array<uint32_t, 2> UMP = UMPMessage::mt3Sysex7(group, length < 7 && state==0 ? 0 : 3, sxPos, sx);
        tud_ump_write(0,UMP.data(),2);
    }
}


void processUMPSysex(struct umpData mess){
    //Example of Processing UMP into MIDI-CI processor
    if(mess.form==1 && mess.data[0] == S7UNIVERSAL_NRT && mess.data[2] == S7MIDICI){
        if(mess.umpGroup==0) {
            MIDICIHandler.startSysex7(mess.umpGroup, mess.data[1]);
            isProcMIDICI = true;
        }
    }
    for (int i = 0; i < mess.dataLength; i++) {
        if(mess.umpGroup==0 && isProcMIDICI){
            MIDICIHandler.processMIDICI(mess.data[i]);
        }else{
            //Process other SysEx
        }
    }
    if((mess.form==3 || mess.form==0) && isProcMIDICI){
        MIDICIHandler.endSysex7();
        isProcMIDICI = false;
    }
}

bool checkMUIDCallback(uint8_t umpGroup, uint32_t muid){
    return (m2procMUID==muid);
}

void recvDiscovery(struct MIDICI ciDetails, std::array<uint8_t, 3> manuId, std::array<uint8_t, 2> familyId,
                           std::array<uint8_t, 2> modelId, std::array<uint8_t, 4> version, uint8_t remoteciSupport,
                           uint16_t remotemaxSysex, uint8_t outputPathId
){
    //All MIDI-CI Devices shall reply to a Discovery message
    printf("Received Discover on Group %d remote MUID: %d\n", ciDetails.umpGroup, ciDetails.remoteMUID);
    uint8_t sysexBuffer[64];
    int len = CIMessage::sendDiscoveryReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION, m2procMUID, ciDetails.remoteMUID,
                                            {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID},
                                            {DEVICE_VERSIONID},0b100,
                                            512, outputPathId,
                                            0 //fbIdx
    );
    sendOutSysex(ciDetails.umpGroup, sysexBuffer ,len, 0);
}

void handleProfileInquiry(MIDICI ciDetails){
    uint8_t profileNone[0] = {};
    uint8_t sysexBuffer[512];
    int len;

    if(ciDetails.deviceId == 0 || ciDetails.deviceId == 0x7F){
        uint8_t profilesEnabled[] = {
                0x7E, 0x21, 0x01, 0x01,0x02,
                0x7E, 0x61, 0x00, 0x01, 0x01
        };
        len = CIMessage::sendProfileListResponse(sysexBuffer,
                MIDICI_MESSAGEFORMATVERSION, m2procMUID, ciDetails.remoteMUID,
                0, 2, profilesEnabled, 0,
                profileNone);
        sendOutSysex(ciDetails.umpGroup, sysexBuffer, len, 0);
    }

    if(ciDetails.deviceId == 0x7F){
        len = CIMessage::sendProfileListResponse(sysexBuffer,MIDICI_MESSAGEFORMATVERSION,
                 m2procMUID, ciDetails.remoteMUID, 0x7F,0,
                 profileNone, 0, profileNone);
        sendOutSysex(ciDetails.umpGroup, sysexBuffer, len, 0 );
    }

}

void handleProfileDetailsInquiry(MIDICI ciDetails, std::array<uint8_t, 5> profile,   uint8_t InquiryTarget){
    uint8_t sysexBuffer[512];
    int len;
    std::array<uint8_t, 5> profileDrawBar {0x7E, 0x21, 0x01, 0x01,0x02};
    if(ciDetails.deviceId == 0 && profileDrawBar==profile && InquiryTarget == 1){
        uint8_t data[1] = {0b1111};
        len = CIMessage::sendProfileDetailsReply(sysexBuffer, MIDICI_MESSAGEFORMATVERSION,
                                                  m2procMUID, ciDetails.remoteMUID, ciDetails.deviceId, profile, InquiryTarget,
                                                  1,data);
        sendOutSysex(ciDetails.umpGroup, sysexBuffer, len, 0);
    }
}

uint32_t random(uint32_t max){
    return (rand() % static_cast<uint32_t>(max + 1));
}

