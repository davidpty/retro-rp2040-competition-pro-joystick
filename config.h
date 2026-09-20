#ifndef JOYSTICK_CONFIG_H_
#define JOYSTICK_CONFIG_H_

#include <stdint.h>

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

/* Button outputs are configured at runtime through the JOYSTICK.INI file on
 * the config-mode USB drive (see ini_config.h). Factory defaults restore the
 * original shipping behavior: Big Fire 1 -> Button 1, Big Fire 2 -> Button 2,
 * Small Fire 1 -> autofire Button 1, Small Fire 2 -> Button 3 (set in
 * settings.c). Button gestures below always use the physical fire buttons and
 * can never be remapped. */
#define JOY_AUTOFIRE_DEFAULT_HZ   20  /* Used only when flash settings are invalid */
#define JOY_AUTOFIRE_MIN_HZ        1
#define JOY_AUTOFIRE_MAX_HZ       60
#define JOY_AUTOFIRE_REPEAT_MS   500  /* Rate adjustment repeat interval */
#define JOY_GESTURE_ACTIVATION_DELAY_MS 500  /* Hold before rate/mode action */
#define JOY_SPECIAL_HOLD_MS        3000  /* Special gesture hold time */

/* Hold Big Fire 1 + Big Fire 2 for JOY_SPECIAL_HOLD_MS (3s): toggle LED
 * feedback.
 *
 * Hold Small Fire 1 + Small Fire 2 to select a mode, then release to enter it.
 * Once JOY_SPECIAL_HOLD_MS (3s) elapses config mode is selected (LED shows
 * JOY_LED_CONFIG_COLOR). Keep holding an additional JOY_CONFIG_MODE_HOLD_MS
 * (3s, 6s total) to select firmware update / BOOTSEL instead (LED shows
 * JOY_LED_FIRMWARE_COLOR). Releasing before 3s selects nothing. */
#define JOY_CONFIG_MODE_HOLD_MS   3000
#define JOY_CONFIG_DRIVE_MAGIC    0x434f4e47u  /* "CONG" in watchdog scratch */
#define JOY_CONFIG_EXIT_GUARD_MAGIC 0x45584954u /* "EXIT" in watchdog scratch */
#define JOY_CONFIG_SAVE_IDLE_MS   1000  /* Drive idle before applying the INI */

/* Hold these two physical buttons to select config / firmware update mode. */
#define JOY_UPDATE_GPIO_A         JOY_GPIO_SMALL_FIRE_1  /* Small Fire Button 1 */
#define JOY_UPDATE_GPIO_B         JOY_GPIO_SMALL_FIRE_2  /* Small Fire Button 2 */

#define JOY_DEBOUNCE_MS            5  /* Stable time before a switch changes */

/* Runtime speed modes. */
#define JOY_FAST_REPORT_INTERVAL_US   1000  /* 1 ms / approximately 1000 Hz */
#define JOY_SLOW_REPORT_INTERVAL_US  80000  /* 80 ms / approximately 12.5 Hz */

/* The PIO sends the upper 24 bits of each 32-bit FIFO word as GGRRBB; the
 * low byte is padding. Use ordinary RGB channel values here so color choices
 * cannot accidentally place a channel in the padding byte. */
#define JOY_LED_RGB(red, green, blue) \
    (((uint32_t)(green) << 24) | \
     ((uint32_t)(red) << 16) | \
     ((uint32_t)(blue) << 8))

#define JOY_LED_IDLE_COLOR      JOY_LED_RGB(0, 0, 0)       /* Off */

/* Profile colors and their joystick-selection order. */
#define JOY_PROFILE_COLOR_0   JOY_LED_RGB(255, 0, 0)       /* Red     - active profile */
#define JOY_PROFILE_COLOR_1   JOY_LED_RGB(0, 255, 0)       /* Green */
#define JOY_PROFILE_COLOR_2   JOY_LED_RGB(100, 0, 255)     /* Purple */
#define JOY_PROFILE_COLOR_3   JOY_LED_RGB(255, 255, 0)     /* Yellow */
#define JOY_PROFILE_COUNT       4
#define JOY_PROFILE_SLOW_BRIGHTNESS 0x33u  /* Slow polling dims the active profile color (20%) */

/* Pure blue is the least visible WS2812 hue, so it is reserved for the least
 * used action: confirming config mode selection. Firmware update is the
 * bright, unmistakable white. */
#define JOY_LED_CONFIG_COLOR   JOY_LED_RGB(0, 0, 255)       /* Blue: config mode selected */
#define JOY_LED_FIRMWARE_COLOR JOY_LED_RGB(0, 51, 255)      /* Dim cyan: firmware update selected */
#define JOY_LED_WARNING_COLOR  JOY_LED_RGB(255, 0, 0)       /* Red: rejected INI indication */

_Static_assert(JOY_LED_RGB(255, 0, 0) == 0x00ff0000u,
               "LED RGB packing must preserve red");
_Static_assert(JOY_LED_RGB(0, 255, 0) == 0xff000000u,
               "LED RGB packing must preserve green");
_Static_assert(JOY_LED_RGB(0, 0, 255) == 0x0000ff00u,
               "LED RGB packing must preserve blue");
_Static_assert(JOY_LED_RGB(0, 255, 255) == 0xff00ff00u,
               "LED RGB packing must preserve cyan");

/* Development USB identity; replace VID/PID before commercial distribution. */
#define JOY_USB_VID           0xcafe
#define JOY_USB_PID           0x4022
#define JOY_USB_BCD_DEVICE    0x0100
#define JOY_USB_MANUFACTURER  "Retro RP2040"
#define JOY_USB_PRODUCT       "Competition Pro"
/* USB serial is the 16-character board ID. */

#endif
