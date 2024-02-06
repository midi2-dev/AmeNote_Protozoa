#include "USBMIDITask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "UMPProcessing.h"
#include "dump_packet.h"

// for USB MIDI interface
#include "ump_device.h"

static void sendPacket(const midi::universal_packet &p)
{
    TRACE_OUTGOING_PACKET("USB MIDI Out", p); 

    tud_ump_write(0, (uint32_t*)p.data, p.size());
}

static UMPProcessing USBMIDI("ProtoZOA USB MIDI", sendPacket);

extern "C" void pvrUSBMIDI(void *pvParameters)
{
    printf("USB MIDI task initialized.\n");

    midi::universal_packet inPacket;
    size_t numMissingWords = 0;

    while (true)
    {
        taskYIELD();

        if (tud_ump_n_mounted(0))
        {
            USBMIDI.sendPendingUMPs();

            // Read and process USB MIDI
            while (auto ump_n_available = tud_ump_n_available(0))
            {
                constexpr uint16_t maxUMPWords = 32;
                uint32_t wordBuffer[maxUMPWords];
                uint32_t wordCount = tud_ump_read(0, wordBuffer, maxUMPWords);
                for (uint32_t w = 0; w < wordCount; ++w)
                {
                    if (numMissingWords == 0)
                    {
                        inPacket.data[0] = wordBuffer[w];
                        numMissingWords = inPacket.size()-1;
                    }
                    else
                    {
                        inPacket.data[inPacket.size() - numMissingWords] = wordBuffer[w];
                        --numMissingWords;
                    }

                    if ((numMissingWords == 0) && (inPacket.data[0] != 0))
                    {
                        TRACE_INCOMING_PACKET("USB MIDI In", inPacket); 
                        USBMIDI.process(inPacket);
                    }
                }
            }
        }
        else
        {
            USBMIDI.clearPendingUMPs();
        }
    }
}

void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    (void) itf;
    printf("UMP on USB enabled: %d \n", (int)alt);
}
