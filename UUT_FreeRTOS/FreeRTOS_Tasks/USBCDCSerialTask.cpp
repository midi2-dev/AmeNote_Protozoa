#include "USBCDCSerialTask.h"

#include "FreeRTOS.h"
#include "task.h"

// for USB MIDI interface
#include "tusb.h"

#include "UMPProcessing.h"
#include "SerialBracketing.h"
#include "dump_packet.h"

#include <stdio.h>

static SerialBracketing bracketing;
static void sendPacket(const midi::universal_packet &p)
{
    if (tud_cdc_connected())
    {
        TRACE_OUTGOING_PACKET("CDC out", p);
        
        uint8_t buffer[p.size()*4 + 2];
        uint8_t length = bracketing.encode(p, buffer);
        for (uint i = 0; i < length; i++)
            tud_cdc_write_char(buffer[i]);
        tud_cdc_write_flush();
    }
}

static UMPProcessing cdcSerial("ProtoZOA CDC", sendPacket);

extern "C" void pvrUSBCDCSerial(void * /*pvParameters*/)
{
    printf("USB CDC Serial task initialized.\n");

    while (1)
    {
        taskYIELD();

        if (tud_cdc_connected())
        {
            cdcSerial.sendPendingUMPs();

            // Read From USB Serial
            char uBuf[1];
            while (tud_cdc_available() && (tud_cdc_read(uBuf, 1) > 0))
            {
                switch (bracketing.feed(uBuf[0]))
                {
                case SerialBracketing::SUCCESS:
                    TRACE_INCOMING_PACKET("CDC in", bracketing.ump);
                    cdcSerial.process(bracketing.ump);
                    break;
                case SerialBracketing::CRC_ERROR:
                    dump_packet("CDC CRC error: ", bracketing.ump);
                  #if PROTOZOA_SERIAL_BRACKET16
                    printf("  expected 0x%02x, is 0x%02x\n", unsigned(uBuf[0]), unsigned(bracketing.checksum()));
                  #endif
                    break;
                case SerialBracketing::INVALID_UMP:
                    dump_packet("CDC Invalid UMP: ", bracketing.ump);
                    break;
                default:
                    break;
                }
            }
        }
        else
        {
            cdcSerial.clearPendingUMPs();
        }
    }
}
