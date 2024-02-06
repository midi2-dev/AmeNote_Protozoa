#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#include "FreeRTOS.h"

// Task Priorities
#define BLINK_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)
#define CME_WIDI_CORE_PRIORITY    (tskIDLE_PRIORITY + 1)
#define DIN_SERIAL_PRIORITY       (tskIDLE_PRIORITY + 1)
#define ETHERNET_W5500_PRIORITY   (tskIDLE_PRIORITY + 1)
#define PICOMAIN_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)
#define TUSB_DEVICE_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define TYPE25_SERIAL_PRIORITY    (tskIDLE_PRIORITY + 1)
#define USB_CDC_SERIAL_PRIORITY   (tskIDLE_PRIORITY + 1)
#define USB_MIDI_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)

enum Groups
{
  MainGroup       = 0x00,
  DINPortsGroup   = 0x01,
 #if PROTOZOA_EXPANSION_CME_WIDI_CORE
  DINPortInGroup  = 0x01,
  CMEWidiGroup    = 0x02,
 #else
  DINPortInGroup  = 0x02,
  DINPortOutGroup = 0x01,
 #endif
};

#endif // FREERTOS_TASKS_H
