/**
 * @brief main.c
 * Main code entry point for ProtoZOA_UUT Processor. This routine will initialize
 * hardware and establish and start FreeRTOS SMP version.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Michael Loh (AmeNote.com)
 * Copyright (c) 2023 Franz Detro (native-instruments.com)
 *
 * FreeRTOS V202107.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

#include "FreeRTOS.h"
#include "FreeRTOS_Tasks.h"
#include "task.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "include/tusb_freertos.h"

#include "BlinkTask.h"
#include "CMEWidiTask.h"
#include "DINSerialTask.h"
#include "EthernetW5500Task.h"
#include "PicoMainTask.h"
#include "Type25SerialTask.h"
#include "USBCDCSerialTask.h"
#include "USBMIDITask.h"

static void prvSetupHardware( void );

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

/**
 * @brief vLaunch main task creation and FreeRTOS start.
 * Creates all tasks for ProtoZOA UUT then launches the FreeRTOS scheduler.
 *
 */
void vLaunch( void)
{
    printf(" Starting ProtoZOA FreeRTOS Tasks.\n");

    // Create task to blink LED
    xTaskCreate(    pvrBlink,                   /* Task function */
                    "Blink",                    /* The text name assigned to task */
                    BLINK_STACK_SIZE,           /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    BLINK_TASK_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );

    // Create DIN Serial MIDI task
    xTaskCreate(    pvrDINSerial,               /* Task function */
                    "DINSerial",                /* The text name assigned to task */
                    DIN_SERIAL_STACK_SIZE,      /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    DIN_SERIAL_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );


#if PROTOZOA_EXPANSION_CME_WIDI_CORE
    xTaskCreate(    pvrCMEWidiCore,                   /* Task function */
                    "WidiCore",                    /* The text name assigned to task */
                    CME_WIDI_CORE_STACK_SIZE,           /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    CME_WIDI_CORE_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );
#elif PROTOZOA_EXPANSION_SERIAL_TYPE25
    xTaskCreate(    pvrType25Serial,                   /* Task function */
                    "Type25Serial",                    /* The text name assigned to task */
                    TYPE25_SERIAL_STACK_SIZE,           /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    TYPE25_SERIAL_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );
#elif PROTOZOA_EXPANSION_ETHERNET_W5500
    xTaskCreate(    pvrEthernetW5500,                   /* Task function */
                    "EthernetW5500",                    /* The text name assigned to task */
                    ETHERNET_W5500_STACK_SIZE,           /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    ETHERNET_W5500_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );
#endif

#if PROTOZOA_USB_CDC_SERIAL

    xTaskCreate(    pvrUSBCDCSerial,                   /* Task function */
                    "USBCDCSerial",                    /* The text name assigned to task */
                    USB_CDC_SERIAL_STACK_SIZE,           /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    USB_CDC_SERIAL_PRIORITY,        /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );
#endif

    // Create USB MIDI task
    xTaskCreate(    pvrUSBMIDI,                 /* Task function */
                    "USBMIDI",                  /* The text name assigned to task */
                    USBMIDI_STACK_SIZE,         /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    USB_MIDI_TASK_PRIORITY,     /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );

    // Create Pico Main task (intercore, controls)
    xTaskCreate(    pvrPicoMain,                /* Task function */
                    "PicoMain",                 /* The text name assigned to task */
                    PICOMAIN_STACK_SIZE,        /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    PICOMAIN_TASK_PRIORITY,     /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );

    tusb_freertos_tud_create( TUSB_DEVICE_STACK_SIZE, TUSB_DEVICE_TASK_PRIORITY );

    vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the Idle and/or
	timer tasks to be created.  See the memory management section on the
	FreeRTOS web site for more details on the FreeRTOS heap
	http://www.freertos.org/a00111.html. */
	for( ;; );
}

/**
 * @brief main() - entry routine for ProtoZOA UUT.
 * The main entry routine for ProtoZOA UUT.
 *
 * Initializes hardware, FreeRTOS and initial tasks.
 * Based on configuration will initialize as multi-core or on
 * specific core of RP2040.
 *
 */
int main( void )
{
    /* Configure the hardware ready to run the demo. */
    prvSetupHardware();
    const char *rtos_name;
#if ( portSUPPORT_SMP == 1 )
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if ( portSUPPORT_SMP == 1 ) && ( configNUM_CORES == 2 )
    printf("%s on both cores:\n", rtos_name);
    vLaunch();
#endif

#if ( mainRUN_ON_CORE == 1 )
    printf("%s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true);
#else
    printf("%s on core 0:\n", rtos_name);
    vLaunch();
#endif

    return 0;
}

/**
 * @brief prvSetupHardware will setup hardware for ProtoZOA UUT
 * Sets up all hardware peripherals for use in ProtoZOA UUT applications.
 *
 */
static void prvSetupHardware( void )
{
    // Initialize stdio for board.
    stdio_init_all();

    // Configure for use of on board LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
    gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    ( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    ;
}
