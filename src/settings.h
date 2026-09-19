#ifndef JOYSTICK_SETTINGS_H_
#define JOYSTICK_SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

#include "joystick.h"

#define JOY_SETTINGS_MAGIC   0x4a535447u
#define JOY_SETTINGS_VERSION 6u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t speed;
    uint8_t rate_hz;
    uint8_t led_enabled;
    uint8_t reserved1;
    uint32_t sequence;
    uint32_t crc32;
} joystick_settings_record_t;

void joystick_settings_defaults(joystick_settings_t *settings);
joystick_settings_record_t joystick_settings_record_make(
    const joystick_settings_t *settings, uint32_t sequence);
bool joystick_settings_record_valid(const joystick_settings_record_t *record);
bool joystick_settings_load_records(const joystick_settings_record_t *first,
                                    const joystick_settings_record_t *second,
                                    joystick_settings_t *settings,
                                    unsigned *slot);

#endif
