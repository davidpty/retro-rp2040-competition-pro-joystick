#include "config_service.h"

#include <string.h>

#include "hardware/flash.h"
#include "ini_config.h"
#include "msc_disk.h"
#include "msc_volume.h"
#include "settings.h"
#include "settings_store.h"

config_apply_result_t config_service_apply(const joystick_settings_t *current) {
    const msc_volume_t *volume = msc_disk_volume();
    uint8_t data[2 * MSC_DISK_BLOCK_SIZE];
    size_t length = msc_volume_read_ini(volume, data, sizeof(data));
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    uint8_t rates[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT] = {{0}};
    config_apply_result_t result = CONFIG_APPLY_REJECTED;

    if (ini_config_parse_with_rates(data, length, bindings, rates)) {
        joystick_settings_t parsed = *current;
        for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
            memcpy(parsed.profiles[p].direction, bindings[p],
                   sizeof(parsed.profiles[p].direction));
            memcpy(parsed.profiles[p].button, bindings[p] + JOY_DIRECTION_COUNT,
                   sizeof(parsed.profiles[p].button));
            memcpy(parsed.autofire_hz[p], rates[p], sizeof(parsed.autofire_hz[p]));
        }
        if (joystick_settings_equal(&parsed, current)) {
            result = CONFIG_APPLY_UNCHANGED;
        } else {
            unsigned slot = 0;
            uint32_t sequence = 0;
            const joystick_settings_record_t *loaded = settings_flash_record(0);
            if (joystick_settings_record_valid(loaded)) {
                sequence = loaded->sequence;
            }
            loaded = settings_flash_record(1);
            if (joystick_settings_record_valid(loaded) &&
                (int32_t)(loaded->sequence - sequence) > 0) {
                slot = 1;
                sequence = loaded->sequence;
            }
            settings_store_t store;
            settings_store_init(&store, slot, sequence);
            settings_store_queue(&store, &parsed);
            if (settings_store_finish(&store)) result = CONFIG_APPLY_CHANGED;
        }
    }
    msc_disk_ack();
    return result;
}
