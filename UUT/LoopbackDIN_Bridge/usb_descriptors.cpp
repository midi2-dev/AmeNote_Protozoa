/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022-2026 Michael Loh (AmeNote.com)
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
#define USB_PID_APP       0x21  // unique per test app -- see UUT/USB_PID_ASSIGNMENTS.md
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | (USB_PID_APP << 8) )

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

  // Use Interface Association Descriptor (IAD) -- two IADs here, one per
  // Audio Control + MIDIStreaming function pair.
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

/**
 * @brief USB MIDI 2.0 Descriptor -- TWO independent MIDIStreaming interfaces
 *
 * Demonstrates CFG_TUD_UMP=2: two Audio-Control+MIDIStreaming interface
 * pairs in one configuration descriptor, each with its own alt-setting 0
 * (legacy MIDI 1.0 byte stream) / alt-setting 1 (UMP) pair, its own bulk
 * endpoints, and its own Group Terminal Block. ump_device.cpp's umpd_open()
 * assigns internal tud_ump interface indices in descriptor-parse order, so:
 *
 *   - Pair A (USB itf 0/1, endpoints 0x01/0x81) = "Loopback"  -> tud_ump itf 0
 *   - Pair B (USB itf 2/3, endpoints 0x02/0x82) = "DIN Bridge" -> tud_ump itf 1
 *
 * See main.cpp for how each tud_ump itf index is driven.
 */
// full speed configuration
#define TOTAL_DESC_LEN  0x0121  // 289 bytes -- see comment above each pair for the byte accounting

uint8_t const desc_fs_configuration[] = {
        // Configuration Descriptor: wTotalLength=0x0121 (289), bNumInterfaces=4,
        // bConfigurationValue=1, bmAttributes=0x80 (bus-powered), bMaxPower=250 (500mA)
        0x09, 0x02, 0x21, 0x01, 0x04, 0x01, 0x00, 0x80, 0x7D,

        // ===================== Pair A: Loopback (tud_ump itf 0) =====================

        // Interface Association Descriptor: groups itf 0-1
        0x08, 0x0B, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,

        // Interface 0: Standard AC (Audio Control) Interface, 0 endpoints
        0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: AC header, bcdADC=1.00,
        // wTotalLength=9, bInCollection=1, baInterfaceNr(1)=1 (MS interface)
        0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01,

        // Interface 1 alt-setting 0: Standard MS Interface, 2 endpoints,
        // iInterface=7 ("MIDI Loopback")
        0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x07,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: MS header, bcdMSC=1.00,
        // wTotalLength=0x41 (65, covers rest of alt-setting 0 and all of alt-setting 1)
        0x07, 0x24, 0x01, 0x00, 0x01, 0x41, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_IN_JACK: embedded IN jack ID=0x01
        0x06, 0x24, 0x02, 0x01, 0x01, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_OUT_JACK: external OUT jack ID=0x11,
        // fed from embedded IN jack ID=0x01, iJack=6 ("Loopback Out")
        0x09, 0x24, 0x03, 0x02, 0x11, 0x01, 0x01, 0x01, 0x06,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_IN_JACK: external IN jack ID=0x02, iJack=5 ("Loopback In")
        0x06, 0x24, 0x02, 0x02, 0x02, 0x05,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_OUT_JACK: embedded OUT jack ID=0x12,
        // fed from external IN jack ID=0x02
        0x09, 0x24, 0x03, 0x01, 0x12, 0x01, 0x02, 0x01, 0x00,

        // Endpoint 0x01 (OUT, bulk, 64 bytes)
        0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
        // CS_ENDPOINT / MIDI_1_CS_ENDPOINT_GENERAL: feeds embedded OUT jack ID=0x12
        0x05, 0x25, 0x01, 0x01, 0x12,
        // Endpoint 0x81 (IN, bulk, 64 bytes)
        0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
        // CS_ENDPOINT / MIDI_1_CS_ENDPOINT_GENERAL: sourced from embedded IN jack ID=0x01
        0x05, 0x25, 0x01, 0x01, 0x01,

        // Interface 1 alt-setting 1: Standard MS Interface, UMP / MIDI 2.0 mode,
        // 2 endpoints, iInterface=7 ("MIDI Loopback")
        0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x03, 0x00, 0x07,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: MS header, bcdMSC=2.00 (UMP),
        // wTotalLength=7 (header only; topology comes from the Group Terminal Block descriptor)
        0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,
        // Endpoint 0x01 (OUT, bulk, 64 bytes)
        0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
        // CS_ENDPOINT / MIDI20_CS_ENDPOINT_GENERAL: associated with Group Terminal Block ID=0x01 (see gtbLoopback)
        0x05, 0x25, 0x02, 0x01, 0x01,
        // Endpoint 0x81 (IN, bulk, 64 bytes)
        0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00,
        // CS_ENDPOINT / MIDI20_CS_ENDPOINT_GENERAL: associated with Group Terminal Block ID=0x01
        0x05, 0x25, 0x02, 0x01, 0x01,

        // ===================== Pair B: DIN Bridge (tud_ump itf 1) =====================

        // Interface Association Descriptor: groups itf 2-3
        0x08, 0x0B, 0x02, 0x02, 0x01, 0x03, 0x00, 0x00,

        // Interface 2: Standard AC (Audio Control) Interface, 0 endpoints
        0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: AC header, baInterfaceNr(1)=3 (MS interface)
        0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x03,

        // Interface 3 alt-setting 0: Standard MS Interface, 2 endpoints,
        // iInterface=11 ("UART DIN Bridge")
        0x09, 0x04, 0x03, 0x00, 0x02, 0x01, 0x03, 0x00, 0x0B,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: MS header, wTotalLength=0x41 (65)
        0x07, 0x24, 0x01, 0x00, 0x01, 0x41, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_IN_JACK: embedded IN jack ID=0x01
        0x06, 0x24, 0x02, 0x01, 0x01, 0x00,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_OUT_JACK: external OUT jack ID=0x11,
        // fed from embedded IN jack ID=0x01, iJack=10 ("DIN Out")
        0x09, 0x24, 0x03, 0x02, 0x11, 0x01, 0x01, 0x01, 0x0A,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_IN_JACK: external IN jack ID=0x02, iJack=9 ("DIN In")
        0x06, 0x24, 0x02, 0x02, 0x02, 0x09,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_OUT_JACK: embedded OUT jack ID=0x12,
        // fed from external IN jack ID=0x02
        0x09, 0x24, 0x03, 0x01, 0x12, 0x01, 0x02, 0x01, 0x00,

        // Endpoint 0x02 (OUT, bulk, 64 bytes)
        0x09, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
        // CS_ENDPOINT / MIDI_1_CS_ENDPOINT_GENERAL: feeds embedded OUT jack ID=0x12
        0x05, 0x25, 0x01, 0x01, 0x12,
        // Endpoint 0x82 (IN, bulk, 64 bytes)
        0x09, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
        // CS_ENDPOINT / MIDI_1_CS_ENDPOINT_GENERAL: sourced from embedded IN jack ID=0x01
        0x05, 0x25, 0x01, 0x01, 0x01,

        // Interface 3 alt-setting 1: Standard MS Interface, UMP / MIDI 2.0 mode,
        // 2 endpoints, iInterface=11 ("UART DIN Bridge")
        0x09, 0x04, 0x03, 0x01, 0x02, 0x01, 0x03, 0x00, 0x0B,
        // CS_INTERFACE / MIDI_1_CS_INTERFACE_HEADER: MS header, wTotalLength=7
        0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,
        // Endpoint 0x02 (OUT, bulk, 64 bytes)
        0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
        // CS_ENDPOINT / MIDI20_CS_ENDPOINT_GENERAL: associated with Group Terminal Block ID=0x01 (see gtbDin)
        0x05, 0x25, 0x02, 0x01, 0x01,
        // Endpoint 0x82 (IN, bulk, 64 bytes)
        0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
        // CS_ENDPOINT / MIDI20_CS_ENDPOINT_GENERAL: associated with Group Terminal Block ID=0x01
        0x05, 0x25, 0x02, 0x01, 0x01,
};

TU_VERIFY_STATIC(sizeof(desc_fs_configuration) == TOTAL_DESC_LEN, "size mismatch");

// Group Terminal Block for USB interface 1 (Pair A, Loopback): one
// bidirectional block spanning all 16 groups, matching tusb_ump_lb's single
// "Loopback" function block -- this interface never talks to the DIN bridge
// half at all, so it's free to claim every group.
uint8_t const gtbLoopback[] = {
        0x05, 0x26, 0x01, 0x12, 0x00,
        0x0D, 0x26, 0x02, 0x01, 0x00, 0x00, 0x10, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00,
};

// Group Terminal Block for USB interface 3 (Pair B, DIN Bridge): one
// bidirectional block covering the single UMP group the DIN port is bridged
// to/from -- matches UartDinBridge::notifyFunctionBlock()'s Discovery
// response for this interface (main.cpp configures it with group=0).
uint8_t const gtbDin[] = {
        0x05, 0x26, 0x01, 0x12, 0x00,
        0x0D, 0x26, 0x02, 0x01, 0x00, 0x00, 0x01, 0x08, 0x11, 0x00, 0x00, 0x00, 0x00,
};

uint8_t const gtbLengths[] = {18, 18};
uint8_t const epInterface[] = {1, 3};      // USB interface numbers of the two MS interfaces
uint8_t const *group_descr[] = {gtbLoopback, gtbDin};
char const* string_desc_arr [] = {
        "",                          // 0: langid
        "AmeNote",                   // 1: Manufacturer
        "USBMidiLoopbackDinBridge",  // 2: Product
        "abcd1234",                  // 3: Serial (overwritten below with board unique ID)
        "Loopback",                  // 4: Pair A Group Terminal Block label
        "Loopback In",               // 5: Pair A external IN jack
        "Loopback Out",              // 6: Pair A external OUT jack
        "MIDI Loopback",             // 7: Pair A MS interface name (alt0+alt1)
        "DIN Bridge",                // 8: Pair B Group Terminal Block label
        "DIN In",                    // 9: Pair B external IN jack
        "DIN Out",                   // 10: Pair B external OUT jack
        "UART DIN Bridge",           // 11: Pair B MS interface name (alt0+alt1)
};
uint8_t const string_desc_arr_length = 11;


// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}


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
  uint8_t chr_count;

  if ( index == 0)
  {
      char const* langSupport = (const char[]) { 0x09, 0x04 };
      memcpy(&_desc_str[1], langSupport, 2);
      chr_count = 1;
  }else if ( index == 3){
      chr_count = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
      char serialId[chr_count];
      pico_get_unique_board_id_string(serialId, chr_count);
      for(uint8_t i=0; i<chr_count; i++)
      {
          _desc_str[1+i] = serialId[i];
      }
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( index > string_desc_arr_length ) return NULL;

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

// Routes a GET_DESCRIPTOR(CS_GR_TRM_BLOCK) request to the Group Terminal
// Block table for whichever USB MIDIStreaming interface (wIndex) it targets
// -- the same epInterface[]/group_descr[]/gtbLengths[] array-dispatch
// pattern UUT/DIN_Bridge uses, just with two entries instead of one.
bool tud_ump_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const * request)
{
  if ( request->wValue == 0x2601 ) //0x26 - CS_GR_TRM_BLOCK 0x01 - alternate interface setting
  {
    uint16_t length = request->wLength;
    uint8_t index = request->wIndex;
    int n =  sizeof(epInterface)/sizeof(epInterface[0]);
    for(int i=0; i<n; i++){
        if(epInterface[i]==index){
            if ( length > gtbLengths[i] ){
                length = gtbLengths[i];
            }
            tud_control_xfer(rhport, request, (void *)group_descr[i], length );
        }
    }

    return true;
  }
  else
    return false;
}
