#if PROTOZOA_EXPANSION_CME_WIDI_CORE

#define CME_WIDI_CORE_STACK_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

void pvrCMEWidiCore(void *pvParameters);

#ifdef __cplusplus
}

#include "UMPRingBuffer.h"

extern UMPRingBuffer<256> CMEWidiSendBuffer;
extern UMPRingBuffer<256> CMEWidiReceiveBuffer;

#endif

#endif
