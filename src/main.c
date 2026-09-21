#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "config.h"
#include "ini_config.h"
#include "joystick.h"
#include "msc_disk.h"
#include "settings.h"
#include "status_led.h"
#include "usb_descriptors.h"

static void init_inputs(void) {
    for (unsigned i = 0; i < INPUT_COUNT; ++i) {
        uint gpio = joystick_input_gpio(i);
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
    }
}

static bool update_buttons_pressed(uint8_t inputs) {
    return joystick_gpio_pressed(inputs, JOY_UPDATE_GPIO_A) &&
           joystick_gpio_pressed(inputs, JOY_UPDATE_GPIO_B);
}

#define SETTINGS_SECTOR_SIZE 4096u
#define SETTINGS_FLASH_BASE (PICO_FLASH_SIZE_BYTES - 2u * SETTINGS_SECTOR_SIZE)
static const joystick_settings_record_t *settings_flash_record(unsigned slot) {
    uintptr_t address = XIP_BASE + SETTINGS_FLASH_BASE + slot * SETTINGS_SECTOR_SIZE;
    return (const joystick_settings_record_t *)address;
}

typedef struct {
    uint32_t offset;
    const uint8_t *page;
} settings_flash_write_t;

static void __not_in_flash_func(settings_flash_write_callback)(void *param) {
    settings_flash_write_t *write = (settings_flash_write_t *)param;
    flash_range_erase(write->offset, SETTINGS_SECTOR_SIZE);
    flash_range_program(write->offset, write->page, FLASH_PAGE_SIZE);
}

static int settings_save_page(uint32_t offset, const uint8_t *page) {
    settings_flash_write_t write = { .offset = offset, .page = page };
    return flash_safe_execute(settings_flash_write_callback, &write, 100);
}

static void queue_settings_save(const joystick_settings_t *settings,
                                uint32_t sequence, unsigned active_slot,
                                uint8_t *page, uint32_t *offset,
                                bool *pending) {
    joystick_settings_record_t record =
        joystick_settings_record_make(settings, sequence + 1u);
    memset(page, 0xff, FLASH_PAGE_SIZE);
    memcpy(page, &record, sizeof(record));
    *offset = SETTINGS_FLASH_BASE + (active_slot ^ 1u) * SETTINGS_SECTOR_SIZE;
    *pending = true;
}

static void finish_pending_save(uint8_t *page, uint32_t offset, bool *pending,
                                unsigned *active_slot, uint32_t *sequence) {
    if (settings_save_page(offset, page) == 0) {
        *active_slot ^= 1u;
        ++*sequence;
        *pending = false;
    }
}

/* Inspect a host-written JOYSTICK.INI from the config drive and classify it.
 * Changed settings are persisted to flash before returning. */
typedef enum {
    CONFIG_APPLY_REJECTED,
    CONFIG_APPLY_UNCHANGED,
    CONFIG_APPLY_CHANGED,
} config_apply_result_t;

static config_apply_result_t config_apply(const joystick_settings_t *current) {
    const msc_volume_t *volume = msc_disk_volume();
    uint8_t data[2 * MSC_DISK_BLOCK_SIZE];
    size_t length = msc_volume_read_ini(volume, data, sizeof(data));
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    config_apply_result_t result = CONFIG_APPLY_REJECTED;
    if (ini_config_parse(data, length, bindings)) {
        joystick_settings_t parsed = *current;
        for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
            memcpy(parsed.profiles[p].direction, bindings[p],
                   sizeof(parsed.profiles[p].direction));
            memcpy(parsed.profiles[p].button, bindings[p] + JOY_DIRECTION_COUNT,
                   sizeof(parsed.profiles[p].button));
        }
        if (joystick_settings_equal(&parsed, current)) {
            result = CONFIG_APPLY_UNCHANGED;
        } else {
            uint8_t page[FLASH_PAGE_SIZE];
            uint32_t offset;
            bool pending;
            unsigned slot = 0;
            uint32_t sequence = 0;
            const joystick_settings_record_t *loaded =
                settings_flash_record(0);
            if (joystick_settings_record_valid(loaded)) {
                slot = 0;
                sequence = loaded->sequence;
            }
            loaded = settings_flash_record(1);
            if (joystick_settings_record_valid(loaded) &&
                (int32_t)(loaded->sequence - sequence) > 0) {
                slot = 1;
                sequence = loaded->sequence;
            }
            queue_settings_save(&parsed, sequence, slot, page, &offset, &pending);
            finish_pending_save(page, offset, &pending, &slot, &sequence);
            if (!pending) result = CONFIG_APPLY_CHANGED;
        }
    }
    msc_disk_ack();
    return result;
}

static void reboot_into_config_mode(void) {
    watchdog_hw->scratch[0] = JOY_CONFIG_DRIVE_MAGIC;
    sleep_ms(20);
    watchdog_reboot(0, 0, 0);
}

int main(void) {
    board_init();
    init_inputs();
    status_led_init();
    bool watchdog_rebooted = watchdog_caused_reboot();

    bool config_drive_enabled =
        watchdog_hw->scratch[0] == JOY_CONFIG_DRIVE_MAGIC;
    bool config_exit_guard =
        watchdog_hw->scratch[0] == JOY_CONFIG_EXIT_GUARD_MAGIC;
    bool post_reboot_input_guard = watchdog_rebooted;
    watchdog_hw->scratch[0] = 0;
    usb_descriptors_set_config_drive(config_drive_enabled);

    input_filter_t filter;
    input_filter_init(&filter, joystick_gpio_snapshot(gpio_get_all()));
    joystick_settings_t settings;
    unsigned settings_slot;
    joystick_settings_load_records(settings_flash_record(0), settings_flash_record(1),
                                    &settings, &settings_slot);
    msc_disk_init(&settings, config_drive_enabled);
    status_led_set_profile(settings.active_profile);
    status_led_set_config_mode(config_drive_enabled);
    /* The startup flash is a power-up indication. Software/watchdog reboots
     * should return directly to the normal idle LED state. */
    if (!watchdog_rebooted) {
        status_led_startup_blink(settings.led_enabled,
                                 settings.speed == JOY_SPEED_SLOW);
    }
    uint32_t settings_sequence = 0;
    const joystick_settings_record_t *loaded_record = settings_flash_record(settings_slot);
    if (joystick_settings_record_valid(loaded_record)) settings_sequence = loaded_record->sequence;

    autofire_state_t autofire;
    autofire_state_init(&autofire);
    autofire.hz = settings.rate_hz;
    boot_mode_state_t boot_mode = {0};
    config_exit_state_t config_exit = {0};
    factory_reset_state_t factory_reset = {0};
    bool factory_reset_led_holdoff = false;
    gesture_state_t gesture_state = {0};
    joystick_keyboard_report_t last_keyboard = {0};
    uint32_t next_report_us = time_us_32();
    bool save_pending = false;
    uint8_t pending_release_mask = 0;
    uint32_t save_retry_us = 0;
    uint32_t pending_save_offset = 0;
    uint8_t pending_save_page[FLASH_PAGE_SIZE];

    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    tusb_init(0, &dev_init);
    if (board_init_after_tusb) board_init_after_tusb();

    while (true) {
        tud_task();
        uint32_t now_us = time_us_32();
        uint8_t inputs = input_filter_update(
            &filter, joystick_gpio_snapshot(gpio_get_all()), now_us);
        post_reboot_input_guard = joystick_post_reboot_guard_active(
            post_reboot_input_guard, inputs);
        bool update_held = update_buttons_pressed(inputs);
        bool small_fire_1 = joystick_input_pressed(inputs, INPUT_SMALL_FIRE_1);
        bool small_fire_2 = joystick_input_pressed(inputs, INPUT_SMALL_FIRE_2);
        bool big_fire_1 = joystick_input_pressed(inputs, INPUT_BIG_FIRE_1);
        bool big_fire_2 = joystick_input_pressed(inputs, INPUT_BIG_FIRE_2);
        bool all_fire_pressed = small_fire_1 && small_fire_2 &&
                                big_fire_1 && big_fire_2;
        if (factory_reset_led_holdoff && !all_fire_pressed) {
            factory_reset_led_holdoff = false;
        }
        bool autofire_adjusting = small_fire_1 && !small_fire_2 &&
                                  (big_fire_1 != big_fire_2);
        if (config_exit_guard && !update_held) config_exit_guard = false;
        bool factory_reset_triggered = false;
        boot_mode_action_t boot_action = BOOT_MODE_NONE;
        if (config_drive_enabled && !post_reboot_input_guard) {
            if (config_mode_exit_step(&config_exit, update_held, now_us)) {
                watchdog_hw->scratch[0] = JOY_CONFIG_EXIT_GUARD_MAGIC;
                sleep_ms(20);
                watchdog_reboot(0, 0, 0);
            }
        } else if (!config_drive_enabled && !config_exit_guard &&
                   !post_reboot_input_guard) {
            if (joystick_gesture_step(&gesture_state, inputs, now_us, &settings)) {
                autofire.hz = settings.rate_hz;
                status_led_set_profile(settings.active_profile);
                pending_release_mask = gesture_state.release_mask;
                queue_settings_save(&settings, settings_sequence, settings_slot,
                                    pending_save_page, &pending_save_offset,
                                    &save_pending);
            }
            factory_reset_triggered = factory_reset_step(
                &factory_reset, all_fire_pressed, now_us);
            if (all_fire_pressed) {
                /* Give the all-four reset priority over the mode selection. */
                boot_mode = (boot_mode_state_t){0};
            }
            if (gesture_state.profile_held || gesture_state.profile_triggered) {
                boot_mode = (boot_mode_state_t){0};
            } else {
                boot_action = boot_mode_step(&boot_mode, update_held, now_us);
            }
            if (boot_action != BOOT_MODE_NONE && save_pending) {
                finish_pending_save(pending_save_page, pending_save_offset,
                                    &save_pending, &settings_slot,
                                    &settings_sequence);
            }
            if (boot_action == BOOT_MODE_CONFIG) {
                reboot_into_config_mode();
            } else if (boot_action == BOOT_MODE_FIRMWARE) {
                status_led_set_hold_color(JOY_LED_FIRMWARE_COLOR);
                status_led_update(false, true, false, now_us);
                sleep_ms(20);
                reset_usb_boot(0, 0);
            }
        }
        if (factory_reset_triggered) {
            joystick_settings_t defaults;
            joystick_settings_defaults(&defaults);
            settings.speed = defaults.speed;
            settings.rate_hz = defaults.rate_hz;
            settings.led_enabled = defaults.led_enabled;
            joystick_settings_reset_profile(&settings, 0);
            joystick_settings_select_profile(&settings, 0);
            status_led_set_profile(settings.active_profile);
            autofire.hz = settings.rate_hz;
            queue_settings_save(&settings, settings_sequence, settings_slot,
                                pending_save_page, &pending_save_offset,
                                &save_pending);
            pending_release_mask = (uint8_t)((1u << INPUT_BIG_FIRE_1) |
                                             (1u << INPUT_BIG_FIRE_2) |
                                             (1u << INPUT_SMALL_FIRE_1) |
                                             (1u << INPUT_SMALL_FIRE_2));
            finish_pending_save(pending_save_page, pending_save_offset,
                                &save_pending, &settings_slot,
                                &settings_sequence);
            msc_disk_rebuild(&settings);
            status_led_startup_blink(true, false);
            factory_reset_led_holdoff = true;
        }

        /* Apply configuration written to the config drive: on idle after the
         * last write, or immediately when the host ejects the volume. A
         * rejected file is reported before rebooting in either case. */
        if (msc_disk_enabled()) {
            bool eject_triggered = msc_disk_ejected();
            bool idle_triggered = msc_disk_modified() &&
                (int32_t)(now_us - msc_disk_last_write_us()) >=
                    JOY_CONFIG_SAVE_IDLE_MS * 1000u;
            bool fire_combo_engaged = (unsigned)big_fire_1 + (unsigned)big_fire_2 +
                                      (unsigned)small_fire_1 + (unsigned)small_fire_2 > 1u;
            if ((eject_triggered || idle_triggered) && !fire_combo_engaged) {
                config_apply_result_t apply_result = config_apply(&settings);
                if (apply_result == CONFIG_APPLY_CHANGED ||
                    (eject_triggered && apply_result == CONFIG_APPLY_UNCHANGED)) {
                    watchdog_reboot(0, 0, 0);
                } else if (apply_result == CONFIG_APPLY_REJECTED) {
                    status_led_rejection_blink();
                    watchdog_reboot(0, 0, 0);
                }
            }
        }

        const joystick_profile_t *active_profile =
            joystick_settings_active_profile(&settings);
        bool autofire_held = !post_reboot_input_guard &&
                             joystick_autofire_enabled(inputs, active_profile);
        uint32_t interval_us = joystick_report_interval_us(settings.speed, autofire_held);
        autofire_state_update(&autofire, autofire_held, now_us);
        bool suppress_fire_output = gesture_state.suppress_output ||
                                    factory_reset.triggered;
        if (!config_drive_enabled && !post_reboot_input_guard) {
            if (joystick_report_due(&next_report_us, now_us, interval_us)) {
                joystick_report_t report = joystick_make_report(
                    &autofire, inputs, active_profile, suppress_fire_output);
                if (tud_hid_n_ready(0)) {
                    tud_hid_n_report(0, REPORT_ID_JOYSTICK, &report, sizeof(report));
                }
            }
            joystick_keyboard_report_t keyboard = joystick_make_keyboard_report(
                &autofire, inputs, active_profile, suppress_fire_output);
            if (memcmp(&keyboard, &last_keyboard, sizeof(keyboard)) != 0) {
                last_keyboard = keyboard;
                if (tud_hid_n_ready(1)) {
                    tud_hid_n_keyboard_report(1, REPORT_ID_KEYBOARD,
                                              keyboard.modifier, keyboard.keycodes);
                }
            }
        }

        uint32_t hold_color = 0;
        if (boot_mode.held) {
            if (boot_mode.mode == BOOT_MODE_FIRMWARE) {
                hold_color = JOY_LED_FIRMWARE_COLOR;
            } else if (boot_mode.mode == BOOT_MODE_CONFIG) {
                hold_color = JOY_LED_CONFIG_COLOR;
            }
        }
        status_led_set_hold_color(hold_color);
        if (!config_drive_enabled && !post_reboot_input_guard &&
            !factory_reset_led_holdoff) {
            bool direct_active = joystick_direct_activity(inputs, autofire_adjusting);
            bool led_active = joystick_status_led_active(direct_active,
                                                         autofire_held,
                                                         autofire.pulse);
            status_led_update(led_active,
                              settings.led_enabled,
                              settings.speed == JOY_SPEED_SLOW, now_us);
        }
        bool gesture_pair_released =
            pending_release_mask != 0 &&
            (inputs & pending_release_mask) != pending_release_mask;
        if (!config_drive_enabled && !post_reboot_input_guard && save_pending &&
            gesture_pair_released &&
            (int32_t)(now_us - save_retry_us) >= 0) {
            finish_pending_save(pending_save_page, pending_save_offset,
                                &save_pending, &settings_slot,
                                &settings_sequence);
            if (save_pending) save_retry_us = now_us + 100000u;
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}
