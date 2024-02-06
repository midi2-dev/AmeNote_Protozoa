#ifndef BLINKTASK_H
#define BLINKTASK_H

#define BLINK_STACK_SIZE configMINIMAL_STACK_SIZE

#ifdef __cplusplus
extern "C" {
#endif

void pvrBlink(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // BLINKTASK_H
