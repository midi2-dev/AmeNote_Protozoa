/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Michael Loh (AmeNote.com)
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
 */

#include "uart_din_bridge.h"

#include <cstdio>
#include <cstring>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "ump_device.h"
#include "include/umpMessageCreate.h"

// Minimal UMP Endpoint Discovery identity. AmeNote does not have a registered
// SysEx manufacturer ID, so this uses the reserved "educational/non-commercial"
// prefix (0x7D) per the MIDI Association spec, matching UUT/DIN_Bridge and
// UUT/USB_FunctionBlocks.
#define DEVICE_MFRID 0x7D,0x00,0x00
#define DEVICE_FAMID 0x00,0x00
#define DEVICE_MODELID 0x00,0x00
#define DEVICE_VERSIONID 0,1,0,0
#define DEVICE_MIDIENDPOINTNAME "AmeNote ProtoZOA UART DIN Bridge"

void UartDinBridge::init(const UartDinBridgeConfig& cfg) {
    _cfg = cfg;
    _rx.defaultGroup = _cfg.group;

    uart_init(_cfg.uart, _cfg.baud);
    gpio_set_function(_cfg.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(_cfg.rx_pin, GPIO_FUNC_UART);
    // DIN MIDI's opto-isolated RX line is open-drain / idle-high and relies
    // on the receiver's own pull-up to establish that idle level -- without
    // this the RX pin floats and no start bit is ever framed. Matches
    // Common/pio_serial/uart_rx.pio's uart_rx_program_init(), which does the
    // same gpio_pull_up() for the PIO-based DIN receiver.
    gpio_pull_up(_cfg.rx_pin);
    uart_set_format(_cfg.uart, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(_cfg.uart, false, false);
    uart_set_fifo_enabled(_cfg.uart, true);

    // Respond to the host's UMP Endpoint/Function Block Discovery requests so
    // it recognizes this bridge as a live MIDI 2.0 UMP endpoint.
    _proc.setMidiEndpoint([this](uint8_t majVer, uint8_t minVer, uint8_t filter) {
        notifyEndpoint(majVer, minVer, filter);
    });
    _proc.setFunctionBlock([this](uint8_t filter, uint8_t fbIdx) {
        notifyFunctionBlock(fbIdx, filter);
    });
}

void UartDinBridge::notifyEndpoint(uint8_t majVer, uint8_t minVer, uint8_t filter) {
    (void) majVer;
    (void) minVer;

    if (filter & 0x1) {
        // 1 function block, MIDI 2.0 and MIDI 1.0 both supported, no JR timestamps.
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointInfoNotify(
                1, true, true, false, false);
        tud_ump_write_hton(_cfg.ump_itf, UMP.data(), 4);
    }

    if (filter & 0x2) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
                {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        tud_ump_write_hton(_cfg.ump_itf, UMP.data(), 4);
    }

    if (filter & 0x4) {
        int nameLength = strlen(DEVICE_MIDIENDPOINTNAME);
        for (uint8_t offset = 0; offset < nameLength; offset += 14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENDPOINTNAME, nameLength);
            tud_ump_write_hton(_cfg.ump_itf, UMP.data(), 4);
        }
    }
}

// This bridge exposes a single bidirectional function block covering the one
// UMP group actually bridged to/from the DIN port (_cfg.group).
void UartDinBridge::notifyFunctionBlock(uint8_t fbIdx, uint8_t filter) {
    if (fbIdx != 0 && fbIdx != 0xFF) return;

    if (filter & 0x1) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                0, true, 3 /*bidirectional*/, false /*sender*/, false /*recv*/,
                _cfg.group /*firstGroup*/, 1 /*groupLength*/, 0x00 /*midiCISupport*/,
                3 /*isMIDI1: bandwidth restricted to 31.25 kbaud, matching DIN*/,
                0 /*maxS8Streams*/);
        tud_ump_write_hton(_cfg.ump_itf, UMP.data(), 4);
    }

    if (filter & 0x2) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                0, 0, (uint8_t *) _cfg.function_block_name, strlen(_cfg.function_block_name));
        tud_ump_write_hton(_cfg.ump_itf, UMP.data(), 4);
    }
}

void UartDinBridge::service() {
    // USB UMP -> DIN bytes
    if (tud_ump_n_mounted(_cfg.ump_itf) && tud_ump_n_available(_cfg.ump_itf)) {
        uint32_t UMPpacket[4];
        uint16_t umpCount = tud_ump_read_ntoh(_cfg.ump_itf, UMPpacket, 4);
        for (uint16_t i = 0; i < umpCount; i++) {
            txUmpWordCount++;
            printf("[%llu] itf%d OUT MIDI%d UMP 0x%08x\n", time_us_64(), _cfg.ump_itf,
                   tud_alt_setting(_cfg.ump_itf) + 1, UMPpacket[i]);

            // Endpoint/Function Block Discovery and other UMP Stream messages
            // are handled here (see notifyEndpoint()/notifyFunctionBlock()
            // above); Channel Voice etc. pass through untouched since no
            // callback is registered for them.
            _proc.processUMP(UMPpacket[i]);

            _tx.UMPStreamParse(UMPpacket[i]);
            while (_tx.availableBS()) {
                uint8_t byte = _tx.readBS();
                if (_tx.group == _cfg.group) {
                    uart_putc_raw(_cfg.uart, byte);
                    txByteCount++;
                }
            }
        }
    }

    // DIN bytes -> USB UMP
    while (uart_is_readable(_cfg.uart)) {
        uint8_t ch = uart_getc(_cfg.uart);
        rxByteCount++;
        lastRxByte = ch;
        if (ch == 0xFE) continue; // Skip Active Sensing
        _rx.bytestreamParse(ch);
        while (_rx.availableUMP()) {
            uint32_t ump = _rx.readUMP();
            rxUmpCount++;
            printf("[%llu] itf%d IN  MIDI%d UMP 0x%08x\n", time_us_64(), _cfg.ump_itf,
                   tud_alt_setting(_cfg.ump_itf) + 1, ump);
            tud_ump_write_hton(_cfg.ump_itf, &ump, 1);
        }
    }
}
