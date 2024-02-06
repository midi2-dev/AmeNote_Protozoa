#if PROTOZOA_USB_CDC_SERIAL

#define USB_CDC_SERIAL_STACK_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

void pvrUSBCDCSerial(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
