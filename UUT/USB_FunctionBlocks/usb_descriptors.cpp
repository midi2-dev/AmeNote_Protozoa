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
        0x09, 0x02, 0xB7, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32, 0x08, 0x0B, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x05, 0x07, 0x24, 0x01, 0x00, 0x01, 0x61, 0x00, 0x06, 0x24, 0x02, 0x01, 0x01, 0x06, 0x09, 0x24, 0x03, 0x02, 0x11, 0x01, 0x01, 0x01, 0x06, 0x06, 0x24, 0x02, 0x02, 0x02, 0x06, 0x09, 0x24, 0x03, 0x01, 0x12, 0x01, 0x02, 0x01, 0x06, 0x06, 0x24, 0x02, 0x01, 0x05, 0x07, 0x09, 0x24, 0x03, 0x02, 0x15, 0x01, 0x05, 0x01, 0x07, 0x06, 0x24, 0x02, 0x02, 0x06, 0x07, 0x09, 0x24, 0x03, 0x01, 0x16, 0x01, 0x06, 0x01, 0x07, 0x09, 0x05, 0x03, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x12, 0x16, 0x09, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x01, 0x05, 0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x03, 0x00, 0x05, 0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00, 0x07, 0x05, 0x03, 0x02, 0x40, 0x00, 0x00, 0x06, 0x25, 0x02, 0x02, 0x01, 0x03, 0x07, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00, 0x06, 0x25, 0x02, 0x02, 0x01, 0x02
};
const uint8_t group_descr[] = {
        0x05, 0x26, 0x01, 0x2C, 0x00, 0x0D, 0x26, 0x02, 0x01, 0x00, 0x00, 0x01, 0x06, 0x11, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x26, 0x02, 0x02, 0x02, 0x01, 0x01, 0x08, 0x01, 0x00, 0x00, 0x01, 0x00, 0x0D, 0x26, 0x02, 0x03, 0x01, 0x02, 0x01, 0x09, 0x01, 0x01, 0x00, 0x00, 0x00
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
                    "ACME Enterprises",                     // 1: Manufacturer
                    "MSynthI",                    // 2: Product
                    serialId,                      // 3: Serials, should use chip ID
                    "ProtoZOA CDC",                // 4: CDC Interface
                    "MSynthI",               // 5: MIDI Interface
                    "Monosynth",               // 6: MIDI Main
                    "External",
                    "Ext IN",             // 7: EXT MIDI IN Jack
                    "Ext OUT",            // 8: EXT MIDI OUT Jack
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


bool tud_ump_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * request)
{
  if ( request->wValue == 0x2601 ) //0x26 - CS_GR_TRM_BLOCK 0x01 - alternate interface setting
  {

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
