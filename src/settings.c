#include "settings.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(joystick_settings_record_t) == 148,
               "Settings record layout changed unexpectedly");

static uint32_t crc32(const void *data, size_t length) {
    const uint8_t *bytes = data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

static void default_profile(joystick_profile_t *profile) {
    memset(profile, 0, sizeof(*profile));
    for (unsigned d = 0; d < JOY_DIRECTION_COUNT; ++d) {
        profile->direction[d].type = INI_BIND_AXIS;
        profile->direction[d].value = d;
    }
    for (unsigned b = 0; b < JOY_BUTTON_COUNT; ++b) {
        profile->button[b].type = INI_BIND_GAMEPAD;
        profile->button[b].value = (uint8_t)(b + 1);
    }
    profile->button[2].autofire = 1;
}

void joystick_settings_defaults(joystick_settings_t *settings) {
    settings->speed = JOY_SPEED_FAST;
    settings->rate_hz = JOY_AUTOFIRE_DEFAULT_HZ;
    settings->led_enabled = true;
    settings->active_profile = 0;
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) default_profile(&settings->profiles[p]);
}

void joystick_settings_select_profile(joystick_settings_t *settings, uint8_t profile) {
    settings->active_profile = profile < JOY_PROFILE_COUNT ? profile : 0;
}

void joystick_settings_sync_active_profile(joystick_settings_t *settings) {
    if (settings->active_profile >= JOY_PROFILE_COUNT) settings->active_profile = 0;
}

const joystick_profile_t *joystick_settings_active_profile(const joystick_settings_t *settings) {
    return &settings->profiles[settings->active_profile < JOY_PROFILE_COUNT ?
                              settings->active_profile : 0];
}

void joystick_settings_reset_profile(joystick_settings_t *settings, uint8_t profile) {
    if (profile < JOY_PROFILE_COUNT) default_profile(&settings->profiles[profile]);
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
        .profiles = {{{{0}}}},
        .active_profile = settings->active_profile,
        .reserved2 = 0,
        .sequence = sequence,
        .crc32 = 0
    };
    memcpy(record.profiles, settings->profiles, sizeof(record.profiles));
    record.crc32 = crc32(&record, offsetof(joystick_settings_record_t, crc32));
    return record;
}

static bool binding_valid(const ini_binding_t *binding) {
    if (binding->autofire > 1 || binding->modifier & ~0x07u) return false;
    switch (binding->type) {
        case INI_BIND_NONE: return binding->value == 0 && binding->modifier == 0;
        case INI_BIND_GAMEPAD: return binding->value >= 1 && binding->value <= 4 && binding->modifier == 0;
        case INI_BIND_AXIS: return binding->value < JOY_DIRECTION_COUNT && binding->modifier == 0;
        case INI_BIND_KEYBOARD: return true;
        default: return false;
    }
}

bool joystick_settings_record_valid(const joystick_settings_record_t *record) {
    if (record->magic != JOY_SETTINGS_MAGIC || record->version != JOY_SETTINGS_VERSION ||
        record->speed > JOY_SPEED_SLOW || record->rate_hz < JOY_AUTOFIRE_MIN_HZ ||
        record->rate_hz > JOY_AUTOFIRE_MAX_HZ || record->led_enabled > 1 ||
        record->active_profile >= JOY_PROFILE_COUNT) return false;
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        for (unsigned i = 0; i < JOY_DIRECTION_COUNT; ++i)
            if (!binding_valid(&record->profiles[p].direction[i])) return false;
        for (unsigned i = 0; i < JOY_BUTTON_COUNT; ++i)
            if (!binding_valid(&record->profiles[p].button[i])) return false;
    }
    return record->crc32 == crc32(record, offsetof(joystick_settings_record_t, crc32));
}

bool joystick_settings_load_records(const joystick_settings_record_t *first,
                                    const joystick_settings_record_t *second,
                                    joystick_settings_t *settings, unsigned *slot) {
    bool fv = joystick_settings_record_valid(first), sv = joystick_settings_record_valid(second);
    if (!fv && !sv) { joystick_settings_defaults(settings); if (slot) *slot = 0; return false; }
    const joystick_settings_record_t *selected;
    unsigned selected_slot;
    if (fv && (!sv || (int32_t)(first->sequence - second->sequence) > 0)) {
        selected = first; selected_slot = 0;
    } else { selected = second; selected_slot = 1; }
    settings->speed = (joystick_speed_t)selected->speed;
    settings->rate_hz = selected->rate_hz;
    settings->led_enabled = selected->led_enabled != 0;
    settings->active_profile = selected->active_profile;
    memcpy(settings->profiles, selected->profiles, sizeof(settings->profiles));
    if (slot) *slot = selected_slot;
    return true;
}

bool joystick_settings_equal(const joystick_settings_t *a, const joystick_settings_t *b) {
    return a->speed == b->speed && a->rate_hz == b->rate_hz &&
           a->led_enabled == b->led_enabled && a->active_profile == b->active_profile &&
           memcmp(a->profiles, b->profiles, sizeof(a->profiles)) == 0;
}
