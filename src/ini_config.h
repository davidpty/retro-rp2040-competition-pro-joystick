#ifndef INI_CONFIG_H_
#define INI_CONFIG_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#define JOY_BUTTON_COUNT 4

/* Persistent, compact button-output codes. Codes are validated by
 * ini_config_parse() and clamped to 0..INI_CODE_MAX when settings load. */
typedef enum {
    INI_CODE_NONE = 0,
    INI_CODE_JOY1 = 1, INI_CODE_JOY2 = 2, INI_CODE_JOY3 = 3, INI_CODE_JOY4 = 4,
    INI_CODE_A = 5,   /* A..Z are contiguous up to 30 */
    INI_CODE_0 = 31,  /* 0..9 are contiguous up to 40 */
    INI_CODE_ENTER = 41, INI_CODE_ESC = 42, INI_CODE_BACKSPACE = 43,
    INI_CODE_TAB = 44, INI_CODE_SPACE = 45,
    INI_CODE_F1 = 46, /* F1..F12 are contiguous up to 57 */
    INI_CODE_SHIFT = 58, INI_CODE_CTRL = 59, INI_CODE_ALT = 60,
    INI_CODE_COUNT = 61,   /* 61 valid codes (0..60) */
    INI_CODE_MAX = INI_CODE_COUNT - 1
} ini_button_code_t;

/* Parse JOYSTICK.INI content. All four color sections must contain four valid
 * button assignments. */
bool ini_config_parse(const uint8_t *data, size_t length,
                      uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT],
                      uint8_t autofire_mask[JOY_PROFILE_COUNT]);

/* Reverse lookup: canonical uppercase name for a code, e.g. "JOY1", "SPACE",
 * "F5"; returns "NONE" for invalid codes. */
const char *ini_config_code_name(uint8_t code);

/* Runtime translation of a stored code. joy_button() returns 1..4 for JOY1..4
 * else 0 (goes to the HID gamepad). keycode() returns the HID usage for keyboard
 * keys else 0. modifier() returns the keyboard modifier bit for SHIFT/CTRL/ALT. */
uint8_t ini_config_joy_button(uint8_t code);
uint8_t ini_config_keycode(uint8_t code);
uint8_t ini_config_modifier(uint8_t code);

#endif
