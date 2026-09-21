#ifndef JOYSTICK_SETTINGS_H_
#define JOYSTICK_SETTINGS_H_

#include <stdbool.h>
#include <stdint.h>

#include "ini_config.h"
#include "joystick_types.h"

#define JOY_SETTINGS_MAGIC   0x4a535447u
#define JOY_SETTINGS_VERSION 11u

#ifndef JOY_SETTINGS_OVERWRITE
#define JOY_SETTINGS_OVERWRITE 0
#endif
#ifndef JOY_SETTINGS_OVERWRITE_TOKEN
#define JOY_SETTINGS_OVERWRITE_TOKEN 0u
#endif

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t speed;
    uint8_t rate_hz;
    uint8_t led_enabled;
    uint8_t reserved1;
    joystick_profile_t profiles[JOY_PROFILE_COUNT];
    uint8_t autofire_hz[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    uint8_t active_profile;
    uint8_t reserved2;
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
bool joystick_settings_equal(const joystick_settings_t *a,
                             const joystick_settings_t *b);
void joystick_settings_select_profile(joystick_settings_t *settings, uint8_t profile);
void joystick_settings_sync_active_profile(joystick_settings_t *settings);
void joystick_settings_reset_profile(joystick_settings_t *settings, uint8_t profile);
const joystick_profile_t *joystick_settings_active_profile(const joystick_settings_t *settings);

#endif
