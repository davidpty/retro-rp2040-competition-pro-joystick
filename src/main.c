#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "config.h"
#include "config_service.h"
#include "joystick_autofire.h"
#include "joystick_gestures.h"
#include "joystick_input.h"
#include "joystick_reports.h"
#include "msc_disk.h"
#include "settings.h"
#include "settings_store.h"
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
    bool settings_loaded = joystick_settings_load_records(
        settings_flash_record(0), settings_flash_record(1), &settings, &settings_slot);
    uint32_t settings_sequence = 0;
    const joystick_settings_record_t *loaded_record = settings_flash_record(settings_slot);
    if (settings_loaded && joystick_settings_record_valid(loaded_record)) {
        settings_sequence = loaded_record->sequence;
    }
    bool overwrite_settings = false;
#if JOY_SETTINGS_OVERWRITE
    if (!settings_loaded || settings.settings_token != JOY_SETTINGS_OVERWRITE_TOKEN) {
        joystick_settings_defaults(&settings);
        settings.settings_token = JOY_SETTINGS_OVERWRITE_TOKEN;
        overwrite_settings = true;
    }
#endif
    msc_disk_init(&settings, config_drive_enabled);
    status_led_set_profile(settings.active_profile);
    status_led_set_config_mode(config_drive_enabled);
    /* The startup flash is a power-up indication. Software/watchdog reboots
     * should return directly to the normal idle LED state. */
    if (!watchdog_rebooted) {
        status_led_startup_blink(settings.led_enabled,
                                 settings.speed == JOY_SPEED_SLOW);
    }
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
    uint8_t pending_release_mask = 0;
    uint32_t save_retry_us = 0;
    settings_store_t settings_store;
    settings_store_init(&settings_store, settings_slot, settings_sequence);

    if (overwrite_settings) {
        settings_store_queue(&settings_store, &settings);
        settings_store_finish(&settings_store);
    }

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
                settings_store_queue(&settings_store, &settings);
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
            if (boot_action != BOOT_MODE_NONE && settings_store_pending(&settings_store)) {
                settings_store_finish(&settings_store);
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
            joystick_settings_reset_profile(&settings, JOY_PROFILE_RED);
            joystick_settings_select_profile(&settings, JOY_PROFILE_RED);
            status_led_set_profile(settings.active_profile);
            autofire.hz = settings.rate_hz;
            settings_store_queue(&settings_store, &settings);
            pending_release_mask = (uint8_t)((1u << INPUT_BIG_FIRE_1) |
                                             (1u << INPUT_BIG_FIRE_2) |
                                             (1u << INPUT_SMALL_FIRE_1) |
                                             (1u << INPUT_SMALL_FIRE_2));
            settings_store_finish(&settings_store);
            msc_disk_rebuild(&settings);
            status_led_startup_blink(true, false);
            factory_reset_led_holdoff = true;
        }

        /* Apply configuration written to the config drive: on idle after the
         * last write, or immediately when the host ejects the volume. A
         * rejected file is reported while keeping config mode available for
         * correction. */
        if (msc_disk_enabled()) {
            bool eject_triggered = msc_disk_ejected();
            bool idle_triggered = msc_disk_modified() &&
                (int32_t)(now_us - msc_disk_last_write_us()) >=
                    JOY_CONFIG_SAVE_IDLE_MS * 1000u;
            bool fire_combo_engaged = (unsigned)big_fire_1 + (unsigned)big_fire_2 +
                                      (unsigned)small_fire_1 + (unsigned)small_fire_2 > 1u;
            if ((eject_triggered || idle_triggered) && !fire_combo_engaged) {
                config_apply_result_t apply_result = config_service_apply(&settings);
                if (apply_result == CONFIG_APPLY_CHANGED ||
                    (eject_triggered && apply_result == CONFIG_APPLY_UNCHANGED)) {
                    watchdog_reboot(0, 0, 0);
                } else if (apply_result == CONFIG_APPLY_REJECTED) {
                    status_led_rejection_blink();
                }
            }
        }

        const joystick_profile_t *active_profile =
            joystick_settings_active_profile(&settings);
        bool suppress_fire_output = gesture_state.suppress_output ||
                                    factory_reset.triggered;
        bool direct_active = joystick_direct_activity(inputs, autofire_adjusting);
        joystick_runtime_output_t runtime = joystick_runtime_step(
            &autofire, inputs, active_profile,
            settings.autofire_hz[settings.active_profile], settings.speed,
            direct_active, suppress_fire_output, now_us);
        if (!config_drive_enabled && !post_reboot_input_guard) {
            if (joystick_report_due(&next_report_us, now_us,
                                    runtime.report_interval_us)) {
                if (tud_hid_n_ready(0)) {
                    tud_hid_n_report(0, REPORT_ID_JOYSTICK, &runtime.joystick,
                                     sizeof(runtime.joystick));
                }
            }
            if (memcmp(&runtime.keyboard, &last_keyboard, sizeof(runtime.keyboard)) != 0 &&
                tud_hid_n_ready(1) &&
                tud_hid_n_keyboard_report(1, REPORT_ID_KEYBOARD,
                                          runtime.keyboard.modifier,
                                          runtime.keyboard.keycodes)) {
                last_keyboard = runtime.keyboard;
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
            bool led_active = runtime.led_active;
            if (gesture_state.rate_adjust_active) {
                led_active = joystick_rate_adjustment_led_step(
                    &gesture_state, settings.rate_hz, now_us);
            }
            status_led_update(led_active,
                              settings.led_enabled,
                              settings.speed == JOY_SPEED_SLOW, now_us);
        }
        bool gesture_pair_released =
            pending_release_mask != 0 &&
            (inputs & pending_release_mask) != pending_release_mask;
        if (!config_drive_enabled && !post_reboot_input_guard &&
            settings_store_pending(&settings_store) &&
            gesture_pair_released &&
            (int32_t)(now_us - save_retry_us) >= 0) {
            settings_store_finish(&settings_store);
            if (settings_store_pending(&settings_store)) save_retry_us = now_us + 100000u;
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
