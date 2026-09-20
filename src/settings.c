#include "settings.h"

#include <stddef.h>
#include <string.h>

#include "config.h"

_Static_assert(sizeof(joystick_settings_record_t) == 40,
               "Settings record must remain a compact 40-byte record");

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
    /* Original shipping behavior: Big Fire 1 -> Button 1, Big Fire 2 ->
     * Button 2, Small Fire 1 -> autofire Button 1, Small Fire 2 -> Button 3. */
    settings->speed = JOY_SPEED_FAST;
    settings->rate_hz = JOY_AUTOFIRE_DEFAULT_HZ;
    settings->led_enabled = true;
    settings->active_profile = 0;
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        settings->profiles[p][0] = INI_CODE_JOY1;
        settings->profiles[p][1] = INI_CODE_JOY2;
        settings->profiles[p][2] = INI_CODE_JOY1;
        settings->profiles[p][3] = INI_CODE_JOY3;
        settings->profile_autofire_mask[p] = 1u << 2;
    }
    memcpy(settings->button_code, settings->profiles[0], JOY_BUTTON_COUNT);
    settings->autofire_mask = settings->profile_autofire_mask[0];
    joystick_settings_select_profile(settings, 0);
}

void joystick_settings_sync_active_profile(joystick_settings_t *settings) {
    if (settings->active_profile >= JOY_PROFILE_COUNT) settings->active_profile = 0;
    memcpy(settings->profiles[settings->active_profile], settings->button_code,
           JOY_BUTTON_COUNT);
    settings->profile_autofire_mask[settings->active_profile] = settings->autofire_mask;
}

void joystick_settings_select_profile(joystick_settings_t *settings, uint8_t profile) {
    joystick_settings_sync_active_profile(settings);
    settings->active_profile = profile < JOY_PROFILE_COUNT ? profile : 0;
    memcpy(settings->button_code, settings->profiles[settings->active_profile],
           JOY_BUTTON_COUNT);
    settings->autofire_mask = settings->profile_autofire_mask[settings->active_profile];
}

void joystick_settings_reset_profile(joystick_settings_t *settings, uint8_t profile) {
    if (profile >= JOY_PROFILE_COUNT) return;
    settings->profiles[profile][0] = INI_CODE_JOY1;
    settings->profiles[profile][1] = INI_CODE_JOY2;
    settings->profiles[profile][2] = INI_CODE_JOY1;
    settings->profiles[profile][3] = INI_CODE_JOY3;
    settings->profile_autofire_mask[profile] = 1u << 2;
    if (settings->active_profile == profile) {
        memcpy(settings->button_code, settings->profiles[profile], JOY_BUTTON_COUNT);
        settings->autofire_mask = settings->profile_autofire_mask[profile];
    }
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
        .profile_button_code = {{0}},
        .profile_autofire_mask = {0},
        .active_profile = settings->active_profile,
        .reserved2 = 0,
        .sequence = sequence,
        .crc32 = 0
    };
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        memcpy(record.profile_button_code[p], settings->profiles[p], JOY_BUTTON_COUNT);
        record.profile_autofire_mask[p] = settings->profile_autofire_mask[p];
    }
    if (record.active_profile < JOY_PROFILE_COUNT) {
        memcpy(record.profile_button_code[record.active_profile], settings->button_code,
               JOY_BUTTON_COUNT);
        record.profile_autofire_mask[record.active_profile] = settings->autofire_mask;
    }
    record.crc32 = crc32(&record, offsetof(joystick_settings_record_t, crc32));
    return record;
}

bool joystick_settings_record_valid(const joystick_settings_record_t *record) {
    if (record->magic != JOY_SETTINGS_MAGIC ||
        record->version != JOY_SETTINGS_VERSION ||
        record->speed > JOY_SPEED_SLOW ||
        record->rate_hz < JOY_AUTOFIRE_MIN_HZ ||
        record->rate_hz > JOY_AUTOFIRE_MAX_HZ ||
        record->led_enabled > 1u || record->active_profile >= JOY_PROFILE_COUNT) return false;
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        if (record->profile_autofire_mask[p] & ~0x0fu) return false;
        for (unsigned i = 0; i < JOY_BUTTON_COUNT; ++i) {
            if (record->profile_button_code[p][i] > INI_CODE_MAX) return false;
        }
    }
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
    settings->active_profile = selected->active_profile;
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        memcpy(settings->profiles[p], selected->profile_button_code[p], JOY_BUTTON_COUNT);
        settings->profile_autofire_mask[p] = selected->profile_autofire_mask[p];
    }
    memcpy(settings->button_code, settings->profiles[settings->active_profile],
           JOY_BUTTON_COUNT);
    settings->autofire_mask = settings->profile_autofire_mask[settings->active_profile];
    if (slot) *slot = selected_slot;
    return true;
}

bool joystick_settings_equal(const joystick_settings_t *a,
                             const joystick_settings_t *b) {
    return a->speed == b->speed && a->rate_hz == b->rate_hz &&
           a->led_enabled == b->led_enabled &&
           a->active_profile == b->active_profile &&
           memcmp(a->profiles, b->profiles, sizeof(a->profiles)) == 0 &&
           memcmp(a->profile_autofire_mask, b->profile_autofire_mask,
                  sizeof(a->profile_autofire_mask)) == 0;
}
