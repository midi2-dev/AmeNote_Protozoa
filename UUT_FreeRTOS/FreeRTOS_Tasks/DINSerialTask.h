#ifndef DINSERIALTASK_H
#define DINSERIALTASK_H

#define DIN_SERIAL_STACK_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

void pvrDINSerial(void *pvParameters);

#ifdef __cplusplus
}

#include "UMPRingBuffer.h"

extern UMPRingBuffer<256> DINPortSendBuffer;
extern UMPRingBuffer<256> DINPortReceiveBuffer;
#endif

#endif // DINSERIALTASK_H
