#include "EthernetW5500Task.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

extern "C" void pvrEthernetW5500(void * /*pvParameters*/)
{
    printf("Ethernet W5500 task initialized.\n");

    while (1)
    {
        // TODO: move Ethernet W5500 code to here
        taskYIELD();
    }

}
