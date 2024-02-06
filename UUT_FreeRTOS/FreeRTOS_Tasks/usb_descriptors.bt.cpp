/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022 Michael Loh (AmeNote.com)
 * Copyright (c) 2022 Franz Detro (native-instruments.de)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "pico/unique_id.h"
#include "ump_device.h"

#include "FreeRTOS_Tasks.h"

#if PROTOZOA_EXPANSION_CME_WIDI_CORE
#define _PID_BT ( 1 << 5 )
#else
#define _PID_BT 0
#endif

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | _PID_BT )

#define USB_VID   0xCafe  // NOTE: Vendor ID is default from TinyUSB and is not valid to be used commercially
#define USB_BCD   0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = USB_BCD,

  // Use Interface Association Descriptor (IAD) for CDC
  // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0040,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_MIDI,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_MIDI        0x03

/**
 * @brief USB MIDI 2.0 Descriptor
 * USB MIDI 2.0 Descriptor for open source project for use by all MIDI Association members.
 * This is a temporary location as the contribution to tinyUSB is developed.
 * 
*/
// initial descriptor with some fixes + Audio Class 2
enum StringDescrIDs
{
  ManufacturerStrID = 0x01,
  ProductStrID      = 0x02,
  SerialStrID       = 0x03,
  CDCSerialStrID    = 0x04,
  USBMIDIStrID      = 0x05,
  MainStrID         = 0x06,
  DINPortStrID      = 0x07,
  CMEWidiStrID      = 0x08,
};

enum MIDI1JackIDs
{
  MIDI1JackMainOutEMB = 0x01,
  MIDI1JackMainInEXT  = 0x02,
  MIDI1JackMainInEMB  = 0x03,
  MIDI1JackMainOutEXT = 0x04,
  MIDI1JackDINOutEMB  = 0x05,
  MIDI1JackDINOutEXT  = 0x06,
  MIDI1JackDINInEXT   = 0x07,
  MIDI1JackDINInEMB   = 0x08,
  MIDI1JackBTOutEMB   = 0x09,
  MIDI1JackBTOutEXT   = 0x0A,
  MIDI1JackBTInEXT    = 0x0B,
  MIDI1JackBTInEMB    = 0x0C,
};

enum MIDI2GrpTrmIDs
{
  MIDI2GrpTrmMain = 0x01,
  MIDI2GrpTrmDIN  = 0x02,
  MIDI2GrpTrmWidi = 0x03,
};

enum
{
  CfgDescrTotalLength = 279,
  CfgDescrTotalLengthLSB = (CfgDescrTotalLength % 256),
  CfgDescrTotalLengthMSB = (CfgDescrTotalLength / 256)
};

// full speed configuration
uint8_t const desc_fs_configuration[] =
{
    // ----- Configuration Descriptor
    0x09,                   // (  9) bLength
    0x02,                   // (  2) bDescriptorType
    CfgDescrTotalLengthLSB, // (   ) wTotalLengthLSB
    CfgDescrTotalLengthMSB, // (   ) wTotalLengthMSB
    ITF_NUM_TOTAL,          // (   ) bNumInterfaces
    0x01,                   // (  1) bConfigurationValue
    0x00,                   // (  0) iConfiguration
    0x80,                   // (128) bmAttributes
    0x32,                   // ( 50) bMaxPower (really??)
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    // ----- Interface Association Descriptor
    0x08,                   // (  8) bLength
    0x0B,                   // ( 11) bDescriptorType
    ITF_NUM_MIDI,           // (   ) bFirstInterface
    0x02,                   // (  2) bInterfaceCount
    0x01,                   // (  1) bFunctionClass
    0x00,                   // (  0) bFunctionSubClass
    0x00,                   // (  0) bFunctionProtocol
    0x00,                   // (  0) iFunction

    // ----- Interface - Audio Control
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI,           // (   ) bInterfaceNumber
    0x00,                   // (  0) bAlternateSetting
    0x00,                   // (  0) bNumEndpoints
    0x01,                   // (  1) bInterfaceClass
    0x01,                   // (  1) bInterfaceSubClass
    0x00,                   // (  0) bInterfaceProtocol
    0x00,                   // (  0) iInterface

    // ----- Audio AC Descriptor - Header
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x00,                   // (  0) bcdACD0
    0x01,                   // (  1) bcdACD1
    0x09,                   // (  9) wTotalLengthLSB
    0x00,                   // (  0) wTotalLengthMSB
    0x01,                   // (  1) Number of streaming interfaces 
    ITF_NUM_MIDI+1,         // (   ) MIDIStreaming Interface

    // ----- Interface - MIDIStreaming
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI+1,         // (   ) bInterfaceNumber
    0x00,                   // (  0) bAlternateSetting
    0x02,                   // (  2) bNumEndpoints
    0x01,                   // (  1) bInterfaceClass
    0x03,                   // (  3) bInterfaceSubClass
    0x00,                   // (  0) bInterfaceProtocol
    USBMIDIStrID,           // (   ) iInterface

    // ----- Audio MS Descriptor - CS Interface - MS Header
    0x07,                   // (  7) bLength
    0x24,                   // ( 36) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x00,                   // (  0) bcdMSC_LSB
    0x01,                   // (  1) bcdMSC_MSB
    30*3+7,                 // ( 97) wTotalLengthLSB
    0x00,                   // (  0) wTotalLengthMSB

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EMB) (Main Out)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackMainOutEMB,    // (   ) bJackID
    MainStrID,              // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EXT) (Main Out)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackMainOutEXT,    // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackMainOutEMB,    // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    MainStrID,              // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EXT) (Main In)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackMainInEXT,     // (   ) bJackID
    MainStrID,              // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EMB) (Main In)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackMainInEMB,     // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackMainInEXT,     // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    MainStrID,              // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EMB) (Ext Out)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackDINOutEMB,     // (   ) bJackID
    DINPortStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EXT) (Ext Out)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackDINOutEXT,     // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackDINOutEMB,     // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    DINPortStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EXT) (Ext In)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackDINInEXT,      // (   ) bJackID
    DINPortStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EMB) (Ext In)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackDINInEMB,      // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackDINInEXT,      // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    DINPortStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EMB) (BT Out)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackBTOutEMB,      // (   ) bJackID
    CMEWidiStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EXT) (BT Out)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackBTOutEXT,      // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackBTOutEMB,      // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    CMEWidiStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EXT) (BT In)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    MIDI1JackBTInEXT,       // (  ) bJackID
    CMEWidiStrID,           // (   ) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EMB) (BT In)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    MIDI1JackBTInEMB,       // (   ) bJackID
    0x01,                   // (  1) bNrInputPins
    MIDI1JackBTInEXT,       // (   ) baSourceID
    0x01,                   // (  1) baSourcePin
    CMEWidiStrID,           // (   ) iJack

    // ----- EP Descriptor - Endpoint - MIDI OUT
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    EPNUM_MIDI,             // (   ) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - EP General
    0x07,                   // (  7) bLength
    0x25,                   // ( 37) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x03,                   // (  3) bNumEmbMIDJack
    MIDI1JackMainOutEMB,    // (   ) baAssocJackID #1
    MIDI1JackDINOutEMB,     // (   ) baAssocJackID #2
    MIDI1JackBTOutEMB,      // (   ) baAssocJackID #3

    // ----- EP Descriptor - Endpoint - MIDI IN
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    0x80+EPNUM_MIDI,        // (129) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General
    0x07,                   // (  7) bLength
    0x25,                   // ( 37) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x03,                   // (  3) bNumEmbMIDJack
    MIDI1JackMainInEMB,     // (   ) baAssocJackID #1
    MIDI1JackDINInEMB,      // (   ) baAssocJackID #2
    MIDI1JackBTInEMB,       // (   ) baAssocJackID #3

    // ----- Interface - MIDIStreaming - Alternate Setting #1
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI+1,         // (   ) bInterfaceNumber
    0x01,                   // (  1) bAlternateSetting
    0x02,                   // (  2) bNumEndpoints
    0x01,                   // (  1) bInterfaceClass
    0x03,                   // (  3) bInterfaceSubClass
    0x00,                   // (  0) bInterfaceProtocol
    0x05,                   // (  5) iInterface

    // ----- Audio MS Descriptor - CS Interface - MS Header
    0x07,                   // (  7) bLength
    0x24,                   // ( 36) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x00,                   // (  0) bcdMSC_LSB
    0x02,                   // (  2) bcdMSC_MSB
    0x07,                   // (  7) wTotalLengthLSB
    0x00,                   // (  0) wTotalLengthMSB

    // ----- EP Descriptor - Endpoint - MIDI OUT
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    EPNUM_MIDI,             // (   ) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General 2.0
    0x07,                   // (  7) bLength
    0x25,                   // ( 37) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x03,                   // (  3) bNumGrpTrmBlock
    MIDI2GrpTrmMain,        // (   ) baAssoGrpTrmBlkID #1
    MIDI2GrpTrmDIN,         // (   ) baAssoGrpTrmBlkID #2
    MIDI2GrpTrmWidi,        // (   ) baAssoGrpTrmBlkID #3

    // ----- EP Descriptor - Endpoint - MIDI IN
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    0x80+EPNUM_MIDI,        // (   ) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General 2.0
    0x07,                   // (  7) bLength
    0x25,                   // ( 37) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x03,                   // (  3) bNumGrpTrmBlock
    MIDI2GrpTrmMain,        // (   ) baAssoGrpTrmBlkID #1
    MIDI2GrpTrmDIN,         // (   ) baAssoGrpTrmBlkID #2
    MIDI2GrpTrmWidi,        // (   ) baAssoGrpTrmBlkID #3
};

constexpr unsigned cgfsize = sizeof(desc_fs_configuration);
static_assert(cgfsize == CfgDescrTotalLength);

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return desc_fs_configuration;

}

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+



static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

    int len = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
    char serialId[len];
    pico_get_unique_board_id_string(serialId, len);
    // array of pointer to string descriptors
    char const* string_desc_arr [] =
            {
                    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
                    "AmeNote",                     // 1: Manufacturer
                    "ProtoZOA",                    // 2: Product
                    serialId,                      // 3: Serials, should use chip ID
                    "ProtoZOA CDC",                // 4: CDC Interface
                    "ProtoZOA MIDI",               // 5: MIDI Interface
                    "ProtoZOA Main",               // 6: MIDI Main
                    "ProtoZOA 5-pin DIN",          // 7: MIDI 5-pin DINs
                    "ProtoZOA CME Widi",           // 8: MIDI CME Widi module
            };

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}


//--------------------------------------------------------------------+
// Group Terminal Block Descriptor
//--------------------------------------------------------------------+

constexpr uint8_t default_protocol =
//0x00; // Unknown (Use MIDI-CI)
0x01; // MIDI 1.0, Support UMP up to 64 bits in size
//0x02; // MIDI 2.0

static midi2_cs_interface_desc_group_terminal_blocks_n_t(3) group_terminal_blocks_descr =
{
  .header = {
    .bLength             = 5,
    .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
    .bDescriptorSubType  = MIDI_GR_TRM_BLOCK_HEADER,
    .wTotalLength        = sizeof(group_terminal_blocks_descr)
  },
  .aBlock = {
    // Main Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = MIDI2GrpTrmMain,
      .bGrpTrmBlkType      = 0x00,   // bi-directional
      .nGroupTrm           = 0x00,
      .nNumGroupTrm        = 1,
      .iBlockItem          = MainStrID,// ProtoZOA Main
      .bMIDIProtocol       = default_protocol,
      .wMaxInputBandwidth  = 0x0000, // Unknown or Not Fixed
      .wMaxOutputBandwidth = 0x0000  // Unknown or Not Fixed
    },
    // 5-PIN DIN Ports Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = MIDI2GrpTrmDIN,
      .bGrpTrmBlkType      = 0x00,   // bi-directional
      .nGroupTrm           = DINPortsGroup,
      .nNumGroupTrm        = 1,
      .iBlockItem          = DINPortStrID,
      .bMIDIProtocol       = 0x01,   // MIDI 1.0, Support UMP up to 64 bits in size
      .wMaxInputBandwidth  = 0x0001, // 31.25kb/s
      .wMaxOutputBandwidth = 0x0001  // 31.25kb/s
    },
    // CME Widi Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = MIDI2GrpTrmWidi,
      .bGrpTrmBlkType      = 0x00,   // bi-directional
      .nGroupTrm           = CMEWidiGroup,
      .nNumGroupTrm        = 1,
      .iBlockItem          = CMEWidiStrID,
      .bMIDIProtocol       = 0x01,   // MIDI 1.0, Support UMP up to 64 bits in size
      .wMaxInputBandwidth  = 0x0000, // Unknown or Not Fixed
      .wMaxOutputBandwidth = 0x0001  // 31.25kb/s
    }
  }
};

bool tud_ump_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * request)
{
  if ( request->wValue == 0x2601 ) //0x26 - CS_GR_TRM_BLOCK 0x01 - alternate interface setting
  {
    const auto & group_descr = group_terminal_blocks_descr;

    uint16_t length = request->wLength;
    if ( length > sizeof( group_descr ) )
    {
      length = sizeof( group_descr );
    }
    tud_control_xfer(rhport, request, (void *)&group_descr, length );
    return true;
  }
  else
    return false;
}
