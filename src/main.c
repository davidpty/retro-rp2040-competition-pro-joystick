#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "config.h"
#include "joystick.h"
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

int main(void) {
    board_init();
    init_inputs();
    status_led_init();

    input_filter_t filter;
    input_filter_init(&filter, joystick_gpio_snapshot(gpio_get_all()));
    joystick_settings_t settings;
    unsigned settings_slot;
    joystick_settings_load_records(settings_flash_record(0), settings_flash_record(1),
                                    &settings, &settings_slot);
    status_led_startup_blink(settings.led_enabled, settings.speed == JOY_SPEED_SLOW);
    uint32_t settings_sequence = 0;
    const joystick_settings_record_t *loaded_record = settings_flash_record(settings_slot);
    if (joystick_settings_record_valid(loaded_record)) settings_sequence = loaded_record->sequence;

    autofire_state_t autofire;
    autofire_state_init(&autofire);
    autofire.hz = settings.rate_hz;
    update_shortcut_t shortcut = {0};
    factory_reset_state_t factory_reset = {0};
    bool factory_reset_led_holdoff = false;
    gesture_state_t gesture_state = {0};
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
        bool suppress_autofire_target_direct = small_fire_1 && !small_fire_2 &&
                                               big_fire_1 && !big_fire_2;
        if (joystick_gesture_step(&gesture_state, inputs, now_us, &settings)) {
            autofire.hz = settings.rate_hz;
            pending_release_mask = gesture_state.release_mask;
            queue_settings_save(&settings, settings_sequence, settings_slot,
                                pending_save_page, &pending_save_offset,
                                &save_pending);
        }
        autofire_state_update(&autofire,
                              joystick_gpio_pressed(inputs, JOY_AUTOFIRE_GPIO), now_us);
        bool factory_reset_triggered = factory_reset_step(
            &factory_reset, all_fire_pressed, now_us);
        if (all_fire_pressed) {
            /* Give the all-four reset priority over BOOTSEL. */
            shortcut = (update_shortcut_t){0};
        } else if (update_shortcut_step(&shortcut, update_held, now_us)) {
            status_led_update(false, false, true, true, false, now_us);
            sleep_ms(20);
            reset_usb_boot(0, 0);
        }
        if (factory_reset_triggered) {
            joystick_settings_defaults(&settings);
            autofire.hz = settings.rate_hz;
            queue_settings_save(&settings, settings_sequence, settings_slot,
                                pending_save_page, &pending_save_offset,
                                &save_pending);
            pending_release_mask = (uint8_t)((1u << INPUT_BIG_FIRE_1) |
                                             (1u << INPUT_BIG_FIRE_2) |
                                             (1u << INPUT_SMALL_FIRE_1) |
                                             (1u << INPUT_SMALL_FIRE_2));
            if (settings_save_page(pending_save_offset, pending_save_page) == 0) {
                settings_slot ^= 1u;
                ++settings_sequence;
                save_pending = false;
            }
            status_led_startup_blink(true, false);
            factory_reset_led_holdoff = true;
        }
        bool autofire_held = joystick_gpio_pressed(inputs, JOY_AUTOFIRE_GPIO);
        uint32_t interval_us = joystick_report_interval_us(settings.speed, autofire_held);
        if (joystick_report_due(&next_report_us, now_us, interval_us)) {
            joystick_report_t report = joystick_make_report(
                &autofire, inputs, now_us, suppress_autofire_target_direct);
            if (tud_hid_ready()) {
                if (tud_hid_report(REPORT_ID_JOYSTICK, &report, sizeof(report))) {
                } else {
                    /* The next deadline will retry with the current input state. */
                }
            }
        }
        if (!factory_reset_led_holdoff) {
            status_led_update(joystick_direct_activity(inputs, autofire_adjusting),
                              joystick_gpio_pressed(inputs, JOY_AUTOFIRE_GPIO) && autofire.pulse,
                              false,
                              settings.led_enabled,
                              settings.speed == JOY_SPEED_SLOW, now_us);
        }
        bool gesture_pair_released =
            pending_release_mask != 0 &&
            (inputs & pending_release_mask) != pending_release_mask;
        if (save_pending && gesture_pair_released &&
            (int32_t)(now_us - save_retry_us) >= 0) {
            if (settings_save_page(pending_save_offset, pending_save_page) == 0) {
                settings_slot ^= 1u;
                ++settings_sequence;
                save_pending = false;
                next_report_us = time_us_32();
            } else {
                save_retry_us = now_us + 100000u;
            }
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
