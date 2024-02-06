#ifndef PICOMAINTASK_H
#define PICOMAINTASK_H

// Stack Sizes
#define TUSB_DEVICE_STACK_SIZE 8192
#define PICOMAIN_STACK_SIZE    2048

#ifdef __cplusplus
extern "C" {
#endif

void pvrPicoMain(void *pvParameters);

#ifdef __cplusplus
}

#include "UMPRingBuffer.h"
extern UMPRingBuffer<32> ControlMessageBuffer;
#endif

#endif // PICOMAINTASK_H
