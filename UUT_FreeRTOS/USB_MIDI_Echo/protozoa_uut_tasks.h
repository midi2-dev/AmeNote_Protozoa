/**
 * @brief protozoa_uut_tasks.h
 * Definitions of ProtoZOA UUT tasks including stack sizes and priorities for
 * FreeRTOS.
 * 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Michael Loh (AmeNote.com)
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
#ifndef PROTOZOA_UUT_TASKS_H
#define PROTOZOA_UUT_TASKS_H

#include "FreeRTOS.h"

// Task Priorities
#define BLINK_TASK_PRIORITY         tskIDLE_PRIORITY + 2
#define TUSB_DEVICE_TASK_PRIORITY   tskIDLE_PRIORITY + 1
#define PROTOZOA_UUT_TASK_PRIORITY  tskIDLE_PRIORITY + 1

// Stack Sizes
#define BLINK_STACK_SIZE            configMINIMAL_STACK_SIZE
#define TUSB_DEVICE_STACK_SIZE      8192
#define PROTOZOA_UUT_STACK_SIZE     2048


#endif // PROTOZOA_UUT_TASKS_H
