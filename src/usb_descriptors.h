#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include <stdbool.h>
#include <stdint.h>

enum { REPORT_ID_JOYSTICK = 0, REPORT_ID_KEYBOARD = 0 };

void usb_descriptors_set_config_drive(bool enable);
bool usb_descriptors_config_drive(void);

#endif