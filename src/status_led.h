#ifndef STATUS_LED_H_
#define STATUS_LED_H_

#include <stdbool.h>
#include <stdint.h>

void status_led_init(void);
void status_led_update(bool direct_active, bool autofire_active,
                       bool upload_active, bool led_enabled, bool slow_mode,
                       uint32_t now_us);
void status_led_startup_blink(bool led_enabled, bool slow_mode);

#endif
