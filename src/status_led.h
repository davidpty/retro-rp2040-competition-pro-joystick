#ifndef STATUS_LED_H_
#define STATUS_LED_H_

#include <stdbool.h>
#include <stdint.h>

void status_led_set_profile(uint8_t index);
void status_led_init(void);
void status_led_update(bool active,
                       bool led_enabled, bool slow_mode, uint32_t now_us);
void status_led_startup_blink(bool led_enabled, bool slow_mode);
void status_led_set_config_mode(bool enabled);
void status_led_rejection_blink(void);
void status_led_set_hold_color(uint32_t color);

#endif
