/**
 * @brief tusb_freertos.c
 * FreeRTOS entry hooks to initialize and use tinyUSB stacks inside FreeRTOS
 * task.
 * 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Michael Loh (AmeNote.com)
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

#include "include/tusb_freertos.h"

// Operational Configuration
#define         TUSB_FREERTOS_DELAY     0                   // Delay period for FreeRTOS task switching between
                                                            //   calls to tud_task(). Note setting 0 just
                                                            //   allows other higher priority tasks or
                                                            //   same priority tasks to run if ready.

// Globals
bool            _tusb_freertos_init = false;                // state of initializing of tinyUSB
TaskHandle_t    _tusb_freertos_tud_task_hdl = NULL;         // task handle
char            _tusb_freertos_tud_name[] = "TUD_TASK";     // task name for tud task

/**
 * @brief tinyUSB Device Task (PRIVATE)
 * tinyUSB Device task - created by tusb_freertos_tud_create.
 * 
 * @param pvParameters not used.
 */
static void tusb_freertos_tud_task( void *pvParameters )
{
    while(1)
    {
        tud_task();
        vTaskDelay( TUSB_FREERTOS_DELAY );
    }
}

/**
 * @brief Initialize tinyUSB Device stack and create task to run.
 * Initializes the tinyUSB stack if needed then creates task to run for USB device management.
 * 
 * @param usStackDepth  task stack depth
 * @param uxPriority    task priority
 * @return true         tinyUSB initialized and task running
 * @return false        error in initialization or task creation
 */
bool tusb_freertos_tud_create( configSTACK_DEPTH_TYPE usStackDepth, UBaseType_t uxPriority )
{
    // If task already running, just return success
    if ( _tusb_freertos_tud_task_hdl )
        return true;

    // Initialize tinyUSB stack if needed       
    if ( !_tusb_freertos_init )
    {
        tusb_init();
        _tusb_freertos_init = true;
    }

    // Create tinyUSB Device management task
    xTaskCreate(    tusb_freertos_tud_task,     /* Task function */
                    _tusb_freertos_tud_name,    /* The text name assigned to task */
                    usStackDepth,               /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    uxPriority,                 /* The priority assigned to the task */
                    &_tusb_freertos_tud_task_hdl /* The task handle, NULL if not required */
                    );

    if ( _tusb_freertos_tud_task_hdl )
        return true;
    else
        return false;
}

