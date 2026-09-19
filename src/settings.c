#include "settings.h"

#include <stddef.h>

#include "config.h"

_Static_assert(sizeof(joystick_settings_record_t) == 20,
               "Settings record must remain a compact 20-byte record");

static uint32_t crc32(const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

void joystick_settings_defaults(joystick_settings_t *settings) {
    settings->speed = JOY_SPEED_FAST;
    settings->rate_hz = JOY_AUTOFIRE_DEFAULT_HZ;
    settings->led_enabled = true;
}

joystick_settings_record_t joystick_settings_record_make(
    const joystick_settings_t *settings, uint32_t sequence) {
    joystick_settings_record_t record = {
        .magic = JOY_SETTINGS_MAGIC,
        .version = JOY_SETTINGS_VERSION,
        .speed = (uint8_t)settings->speed,
        .rate_hz = settings->rate_hz,
        .led_enabled = settings->led_enabled ? 1u : 0u,
        .reserved1 = 0,
        .sequence = sequence,
        .crc32 = 0
    };
    record.crc32 = crc32(&record, offsetof(joystick_settings_record_t, crc32));
    return record;
}

bool joystick_settings_record_valid(const joystick_settings_record_t *record) {
    if (record->magic != JOY_SETTINGS_MAGIC ||
        record->version != JOY_SETTINGS_VERSION ||
        record->speed > JOY_SPEED_SLOW ||
        record->rate_hz < JOY_AUTOFIRE_MIN_HZ ||
        record->rate_hz > JOY_AUTOFIRE_MAX_HZ ||
        record->led_enabled > 1u) return false;
    return record->crc32 == crc32(record, offsetof(joystick_settings_record_t, crc32));
}

bool joystick_settings_load_records(const joystick_settings_record_t *first,
                                    const joystick_settings_record_t *second,
                                    joystick_settings_t *settings,
                                    unsigned *slot) {
    bool first_valid = joystick_settings_record_valid(first);
    bool second_valid = joystick_settings_record_valid(second);
    if (!first_valid && !second_valid) {
        joystick_settings_defaults(settings);
        if (slot) *slot = 0;
        return false;
    }
    const joystick_settings_record_t *selected;
    unsigned selected_slot;
    if (first_valid && (!second_valid || (int32_t)(first->sequence - second->sequence) > 0)) {
        selected = first; selected_slot = 0;
    } else {
        selected = second; selected_slot = 1;
    }
    settings->speed = (joystick_speed_t)selected->speed;
    settings->rate_hz = selected->rate_hz;
    settings->led_enabled = selected->led_enabled != 0;
    if (slot) *slot = selected_slot;
    return true;
}
