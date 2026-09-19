#ifndef JOYSTICK_CONFIG_H_
#define JOYSTICK_CONFIG_H_

/* Switches connect their GPIO to GND when pressed; internal pull-ups are used. */
#define JOY_GPIO_UP             0  /* Joystick Up */
#define JOY_GPIO_DOWN           1  /* Joystick Down */
#define JOY_GPIO_LEFT           2  /* Joystick Left */
#define JOY_GPIO_RIGHT          3  /* Joystick Right */
#define JOY_GPIO_BIG_FIRE_1     4  /* Big Fire Button 1 */
#define JOY_GPIO_BIG_FIRE_2     5  /* Big Fire Button 2 */
#define JOY_GPIO_SMALL_FIRE_1   6  /* Small Fire Button 1 */
#define JOY_GPIO_SMALL_FIRE_2   7  /* Small Fire Button 2 */
#define JOY_GPIO_STATUS_LED    16  /* Onboard WS2812 LED */

/* USB button numbers: 1-3; use 0 to leave a physical button unmapped. */
#define JOY_BUTTON_BIG_FIRE_1    1  /* Big Fire Button 1 -> USB Button 1 */
#define JOY_BUTTON_BIG_FIRE_2    2  /* Big Fire Button 2 -> USB Button 2 */
#define JOY_BUTTON_SMALL_FIRE_1  0  /* Small Fire Button 1 -> autofire only */
#define JOY_BUTTON_SMALL_FIRE_2  3  /* Small Fire Button 2 -> USB Button 3 */

/* Autofire: choose a physical GPIO above and a USB button number (1-3). */
#define JOY_AUTOFIRE_GPIO          JOY_GPIO_SMALL_FIRE_1
#define JOY_AUTOFIRE_USB_BUTTON    1
#define JOY_AUTOFIRE_DEFAULT_HZ   20  /* Used only when flash settings are invalid */
#define JOY_AUTOFIRE_MIN_HZ        1
#define JOY_AUTOFIRE_MAX_HZ       60
#define JOY_AUTOFIRE_REPEAT_MS   500  /* Rate adjustment repeat interval */
#define JOY_GESTURE_ACTIVATION_DELAY_MS 500  /* Hold before rate/mode action */
#define JOY_SPECIAL_HOLD_MS        3000  /* Special gesture hold time */

/* Hold these two physical buttons to enter BOOTSEL firmware update mode. */
#define JOY_UPDATE_GPIO_A         JOY_GPIO_SMALL_FIRE_1  /* Small Fire Button 1 */
#define JOY_UPDATE_GPIO_B         JOY_GPIO_SMALL_FIRE_2  /* Small Fire Button 2 */
#define JOY_UPDATE_GRACE_MS       30  /* Brief release allowed during hold */

#define JOY_DEBOUNCE_MS            5  /* Stable time before a switch changes */

/* Runtime speed modes. */
#define JOY_FAST_REPORT_INTERVAL_US   1000  /* 1 ms / approximately 1000 Hz */
#define JOY_SLOW_REPORT_INTERVAL_US  80000  /* 80 ms / approximately 12.5 Hz */

/* WS2812 colors are GRB, most significant byte first. */
#define JOY_LED_IDLE_COLOR      0x00000000u  /* Off */
#define JOY_LED_ACTIVE_COLOR    0x00330000u  /* 20% red: any input held */
#define JOY_LED_UPDATE_COLOR    0x4d000000u  /* 30% green: update buttons held */
#define JOY_LED_SLOW_COLOR      0x1a00ff00u  /* 10% green / 100% blue cyan */

/* Development USB identity; replace VID/PID before commercial distribution. */
#define JOY_USB_VID           0xcafe
#define JOY_USB_PID           0x4022
#define JOY_USB_BCD_DEVICE    0x0100
#define JOY_USB_MANUFACTURER  "Retro RP2040"
#define JOY_USB_PRODUCT       "Competition Pro"
/* USB serial is the 16-character board ID. */

#endif
