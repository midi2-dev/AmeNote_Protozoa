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

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

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
// full speed configuration
uint8_t const desc_fs_configuration[] =
{
    // ----- Configuration Descriptor
    0x09,                   // (  9) bLength
    0x02,                   // (  2) bDescriptorType
    243, //179+(CFG_TUD_CDC * TUD_CDC_DESC_LEN), // (179 + CDC) wTotalLengthLSB
    0x00,                   // (  0) wTotalLengthMSB
    ITF_NUM_TOTAL,          // (  4) bNumInterfaces
    0x01,                   // (  1) bConfigurationValue
    0x00,                   // (  0) iConfiguration
    0x80,                   // (128) bmAttributes
    0x32,                   // ( 50) bMaxPower (really??)
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    // ----- Interface Association Descriptor
    0x08,                   // (  8) bLength
    0x0B,                   // ( 11) bDescriptorType
    ITF_NUM_MIDI,           // (  2) bFirstInterface
    0x02,                   // (  2) bInterfaceCount
    0x01,                   // (  1) bFunctionClass
    0x03,                   // (  0) bFunctionSubClass
    0x00,                   // (  0) bFunctionProtocol
    0x00,                   // (  0) iFunction

    // ----- Interface - Audio Control
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI,           // (  2) bInterfaceNumber
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
    ITF_NUM_MIDI+1,         // (  3) MIDIStreaming Interface

    // ----- Interface - MIDIStreaming
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI+1,         // (  3) bInterfaceNumber
    0x00,                   // (  0) bAlternateSetting
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
    0x01,                   // (  1) bcdMSC_MSB
    0x5D,                   // ( 93) wTotalLengthLSB
    0x00,                   // (  0) wTotalLengthMSB

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EMB) (Main Out)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    0x01,                   // (  1) bJackID
    0x06,                   // (  6) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EXT) (Main Out)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    0x04,                   // (  4) bJackID
    0x01,                   // (  1) bNrInputPins
    0x01,                   // (  1) baSourceID
    0x01,                   // (  1) baSourcePin
    0x06,                   // (  6) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EXT) (Main In)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    0x02,                   // (  2) bJackID
    0x06,                   // (  6) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EMB) (Main In)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    0x03,                   // (  3) bJackID
    0x01,                   // (  1) bNrInputPins
    0x02,                   // (  2) baSourceID
    0x01,                   // (  1) baSourcePin
    0x06,                   // (  6) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EMB) (Ext Out)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    0x05,                   // (  5) bJackID
    0x08,                   // (  8) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EXT) (Ext Out)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    0x06,                   // (  6) bJackID
    0x01,                   // (  1) bNrInputPins
    0x05,                   // (  5) baSourceID
    0x01,                   // (  1) baSourcePin
    0x08,                   // (  8) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI IN Jack (EXT) (Ext In)
    0x06,                   // (  6) bLength
    0x24,                   // ( 36) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x02,                   // (  2) bJackType
    0x07,                   // (  7) bJackID
    0x07,                   // (  7) iJack

    // ----- Audio MS Descriptor - CS Interface - MIDI OUT Jack (EMB) (Ext In)
    0x09,                   // (  9) bLength
    0x24,                   // ( 36) bDescriptorType
    0x03,                   // (  3) bDescriptorSubtype
    0x01,                   // (  1) bJackType
    0x08,                   // (  8) bJackID
    0x01,                   // (  1) bNrInputPins
    0x07,                   // (  7) baSourceID
    0x01,                   // (  1) baSourcePin
    0x07,                   // (  7) iJack

    // ----- EP Descriptor - Endpoint - MIDI OUT
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    EPNUM_MIDI,             // (  1) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - EP General
    0x06,                   // (  6) bLength
    0x25,                   // ( 37) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x02,                   // (  2) bNumEmbMIDJack
    0x01,                   // (  1) baAssocJackID #1 (Main Out)
    0x05,                   // ( 42) baAssocJackID #2 (Ext Out)

    // ----- EP Descriptor - Endpoint - MIDI IN
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    0x80+EPNUM_MIDI,        // (129) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General
    0x06,                   // (  6) bLength
    0x25,                   // ( 37) bDescriptorType
    0x01,                   // (  1) bDescriptorSubtype
    0x02,                   // (  2) bNumEmbMIDJack
    0x03,                   // (  3) baAssocJackID #1 (Main In)
    0x08,                   // (  8) baAssocJackID #2 (Ext In)

    // ----- Interface - MIDIStreaming - Alternate Setting #1
    0x09,                   // (  9) bLength
    0x04,                   // (  4) bDescriptorType
    ITF_NUM_MIDI+1,         // (  1) bInterfaceNumber
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
    EPNUM_MIDI,             // (  1) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General 2.0
    0x05,                   // (  6) bLength
    0x25,                   // ( 37) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  2) bNumGrpTrmBlock
    0x01,                   // (  1) baAssoGrpTrmBlkID #1 - ProtoZOA Main
    //0x03,                   // (  3) baAssoGrpTrmBlkID #2 - ProtoZOA Ext OUT

    // ----- EP Descriptor - Endpoint - MIDI IN
    0x07,                   // (  7) bLength
    0x05,                   // (  5) bDescriptorType
    0x80+EPNUM_MIDI,        // (129) bEndpointAddress
    0x02,                   // (  2) bmAttributes
    0x40,                   // (  0) wMaxPacketSizeLSB
    0x00,                   // (  2) wMaxPacketSizeMSB
    0x00,                   // (  0) bInterval

    // ----- Audio MS Descriptor - CS Endpoint - MS General 2.0
    0x05,                   // (  6) bLength
    0x25,                   // ( 37) bDescriptorType
    0x02,                   // (  2) bDescriptorSubtype
    0x01,                   // (  2) bNumGrpTrmBlock
    0x01,                   // (  1) baAssoGrpTrmBlkID #1 - ProtoZOA Main
    //0x02,                   // (  2) baAssoGrpTrmBlkID #2 - ProtoZOA Ext IN
};

constexpr unsigned cgfsize = sizeof(desc_fs_configuration);

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
                    "ProtoZOA Ext IN",             // 7: EXT MIDI IN Jack
                    "ProtoZOA Ext OUT",            // 8: EXT MIDI OUT Jack
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
0x03; // MIDI 1.0, Support UMP up to 128 bits in size
//0x11; // MIDI 2.0 128 bits

static midi2_cs_interface_desc_group_terminal_blocks_n_t(1) group_terminal_blocks_desc =
{
  .header = {
    .bLength             = 5,
    .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
    .bDescriptorSubType  = MIDI_GR_TRM_BLOCK_HEADER,
    .wTotalLength        = sizeof(group_terminal_blocks_desc)
  },
  .aBlock = {
    // Main Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = 1,
      .bGrpTrmBlkType      = 0x00,   // bi-directional
      .nGroupTrm           = 0x00,
      .nNumGroupTrm        = 16,
      .iBlockItem          = 0x00,   // No Label
      .bMIDIProtocol       = default_protocol,
      .wMaxInputBandwidth  = 0x0000, // Unknown or Not Fixed
      .wMaxOutputBandwidth = 0x0000  // Unknown or Not Fixed
    }/*,
    // 5-PIN DIN In Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = 2,
      .bGrpTrmBlkType      = 0x02,   // OUT Group Terminals only
      .nGroupTrm           = 0x04,
      .nNumGroupTrm        = 1,
      .iBlockItem          = 0x07,   // ProtoZOA Ext IN
      .bMIDIProtocol       = 0x01,   // MIDI 1.0, Support UMP up to 64 bits in size
      .wMaxInputBandwidth  = 0x0000, // Unknown or Not Fixed
      .wMaxOutputBandwidth = 0x0001  // 31.25kb/s
    },
    // 5-PIN DIN Out Function Block
    {
      .bLength             = 13,
      .bDescriptorType     = MIDI_CS_INTERFACE_GR_TRM_BLOCK,
      .bDescriptorSubType  = MIDI_GR_TRM_BLOCK,
      .bGrpTrmBlkID        = 3,
      .bGrpTrmBlkType      = 0x01,   // IN Group Terminals only
      .nGroupTrm           = 0x05,
      .nNumGroupTrm        = 1,
      .iBlockItem          = 0x08,   // ProtoZOA Ext OUT
      .bMIDIProtocol       = 0x01,   // MIDI 1.0, Support UMP up to 64 bits in size
      .wMaxInputBandwidth  = 0x0001, // 31.25kb/s
      .wMaxOutputBandwidth = 0x0000  // Unknown or Not Fixed
    }*/
  }
};

bool tud_ump_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * request)
{
  if ( request->wValue == 0x2601 ) //0x26 - CS_GR_TRM_BLOCK 0x01 - alternate interface setting
  {
    const auto & group_descr = group_terminal_blocks_desc;

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
