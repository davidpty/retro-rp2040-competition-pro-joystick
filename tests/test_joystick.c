#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "joystick.h"
#include "settings.h"

#define PRESSED(input) (1u << (input))

static void test_gpio_snapshot(void) {
    uint32_t levels = 0xffffffffu & ~(1u << JOY_GPIO_UP) &
                      ~(1u << JOY_GPIO_SMALL_FIRE_2);
    assert(joystick_gpio_snapshot(levels) ==
           (PRESSED(INPUT_UP) | PRESSED(INPUT_SMALL_FIRE_2)));
    assert(joystick_gpio_pressed(PRESSED(INPUT_BIG_FIRE_1), JOY_GPIO_BIG_FIRE_1));
    assert(!joystick_gpio_pressed(PRESSED(INPUT_BIG_FIRE_1), JOY_GPIO_BIG_FIRE_2));
    assert(JOY_UPDATE_GPIO_A == JOY_GPIO_SMALL_FIRE_1);
    assert(JOY_UPDATE_GPIO_B == JOY_GPIO_SMALL_FIRE_2);
}

static void test_debounce(void) {
    input_filter_t filter;
    input_filter_init(&filter, 0);
    uint8_t up = PRESSED(INPUT_UP);
    assert(input_filter_update(&filter, up, 1000) == 0);
    assert(input_filter_update(&filter, up, 5999) == 0);
    assert(input_filter_update(&filter, 0, 6000) == 0); /* Short glitch. */
    assert(input_filter_update(&filter, up, 7000) == 0);
    assert(input_filter_update(&filter, up, 12000) == up);
    assert(input_filter_update(&filter, 0, 13000) == up);
    assert(input_filter_update(&filter, 0, 18000) == 0);

    /* Independent switches debounce independently, including over clock wrap. */
    input_filter_init(&filter, 0);
    uint8_t two = PRESSED(INPUT_UP) | PRESSED(INPUT_BIG_FIRE_2);
    assert(input_filter_update(&filter, two, UINT32_MAX - 2000u) == 0);
    assert(input_filter_update(&filter, two, 2999u) == two);
}

static void test_report_mapping(void) {
    autofire_state_t state = {0};
    uint8_t inputs = PRESSED(INPUT_UP) | PRESSED(INPUT_LEFT) |
                     PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2) |
                     PRESSED(INPUT_SMALL_FIRE_2);
    joystick_report_t report = joystick_make_report(&state, inputs, 0, false);
    assert(sizeof(report) == 3);
    assert(report.x == -127 && report.y == -127 && report.buttons == 7);
    report = joystick_make_report(&state, inputs | PRESSED(INPUT_DOWN) |
                                  PRESSED(INPUT_RIGHT), 1000, false);
    assert(report.x == 0 && report.y == 0 && report.buttons == 7);
    report = joystick_make_report(&state, 0, 2000, false);
    assert(report.x == 0 && report.y == 0 && report.buttons == 0);
    uint8_t const *bytes = (uint8_t const *)&report;
    assert(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0);
}

static void test_autofire(void) {
    autofire_state_t state;
    autofire_state_init(&state);
    const uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    uint8_t trigger = PRESSED(INPUT_SMALL_FIRE_1);
    assert(joystick_make_report(&state, trigger, 1000, false).buttons == 1);
    assert(joystick_make_report(&state, trigger, 1000 + half - 1, false).buttons == 1);
    assert(joystick_make_report(&state, trigger, 1000 + half, false).buttons == 0);
    assert(joystick_make_report(&state, trigger | PRESSED(INPUT_BIG_FIRE_1),
                                1000 + half + 1, false).buttons == 1);
    assert(joystick_make_report(&state, trigger | PRESSED(INPUT_BIG_FIRE_1),
                                1000 + half + 2, true).buttons == 0);
    assert(joystick_make_report(&state, trigger, 1000 + 3 * half, false).buttons == 0);
    assert(joystick_make_report(&state, trigger, 1000 + 4 * half, false).buttons == 1);
    assert(joystick_make_report(&state, 0, 1000 + 4 * half + 1, false).buttons == 0);
    assert(joystick_make_report(&state, trigger, 1000 + 4 * half + 2, false).buttons == 1);

    /* The increase gesture keeps Big Fire 2 mapped while Button 1 autofires. */
    autofire_state_init(&state);
    uint8_t increase = trigger | PRESSED(INPUT_BIG_FIRE_2);
    assert(joystick_make_report(&state, increase, 2000, false).buttons == 3);

    autofire_state_init(&state);
    state.hz = 15;
    autofire_state_update(&state, true, 1000);
    assert(state.pulse);
    autofire_state_update(&state, true, 1000 + half - 1);
    assert(state.pulse);
    autofire_state_update(&state, true, 1000 + half);
    assert(!state.pulse);
    autofire_state_update(&state, false, 1000 + half + 1);
    assert(!state.active && !state.pulse);
}

static void test_direct_activity(void) {
    assert(joystick_direct_activity(PRESSED(INPUT_UP), false));
    assert(joystick_direct_activity(PRESSED(INPUT_BIG_FIRE_1), false));
    assert(!joystick_direct_activity(PRESSED(INPUT_BIG_FIRE_1), true));
    assert(joystick_direct_activity(PRESSED(INPUT_UP) | PRESSED(INPUT_BIG_FIRE_1), true));
    assert(!joystick_direct_activity(PRESSED(INPUT_UP) | PRESSED(INPUT_DOWN), false));
    assert(!joystick_direct_activity(0, false));
}

static void test_gestures(void) {
    gesture_state_t state = {0};
    joystick_settings_t settings = { JOY_SPEED_FAST, 15, true };
    uint8_t down = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_1);
    uint8_t up = PRESSED(INPUT_BIG_FIRE_2) | PRESSED(INPUT_SMALL_FIRE_1);
    uint8_t slow = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_2);
    uint8_t fast = PRESSED(INPUT_BIG_FIRE_2) | PRESSED(INPUT_SMALL_FIRE_2);

    uint32_t gesture_delay = JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u;
    assert(!joystick_gesture_step(&state, down, 0, &settings));
    assert(settings.rate_hz == 15);
    assert(!joystick_gesture_step(&state, down, gesture_delay - 1, &settings));
    assert(settings.rate_hz == 15);
    assert(joystick_gesture_step(&state, down, gesture_delay, &settings));
    assert(settings.rate_hz == 14);
    assert(!joystick_gesture_step(&state, down, gesture_delay + 199999, &settings));
    assert(joystick_gesture_step(&state, down, gesture_delay + 200000, &settings));
    assert(settings.rate_hz == 13);
    assert(!joystick_gesture_step(&state, 0, gesture_delay + 100001, &settings));
    assert(!joystick_gesture_step(&state, up, gesture_delay + 100002, &settings));
    assert(joystick_gesture_step(&state, up,
                                 gesture_delay + 100002 + gesture_delay, &settings));
    assert(settings.rate_hz == 14);

    settings.rate_hz = JOY_AUTOFIRE_MIN_HZ;
    state = (gesture_state_t){0};
    assert(!joystick_gesture_step(&state, down, 0, &settings));
    assert(settings.rate_hz == JOY_AUTOFIRE_MIN_HZ);
    state = (gesture_state_t){0}; settings.rate_hz = JOY_AUTOFIRE_MAX_HZ;
    assert(!joystick_gesture_step(&state, up, 0, &settings));
    assert(settings.rate_hz == JOY_AUTOFIRE_MAX_HZ);

    state = (gesture_state_t){0}; settings.speed = JOY_SPEED_FAST;
    assert(!joystick_gesture_step(&state, slow, 0, &settings));
    assert(!joystick_gesture_step(&state, slow, gesture_delay - 1, &settings));
    assert(joystick_gesture_step(&state, slow, gesture_delay, &settings));
    assert(settings.speed == JOY_SPEED_SLOW);
    assert(!joystick_gesture_step(&state, slow, 1, &settings));
    assert(!joystick_gesture_step(&state, 0, 2, &settings));
    assert(!joystick_gesture_step(&state, fast, 3, &settings));
    assert(joystick_gesture_step(&state, fast, 3 + gesture_delay, &settings));
    assert(settings.speed == JOY_SPEED_FAST);

    /* The small-button pair has no rate/mode action; BOOTSEL handles it separately. */
    state = (gesture_state_t){0}; settings.rate_hz = 15;
    uint8_t small_pair = PRESSED(INPUT_SMALL_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_2);
    uint8_t three = small_pair | PRESSED(INPUT_BIG_FIRE_1);
    assert(!joystick_gesture_step(&state, small_pair, 0, &settings));
    assert(!joystick_gesture_step(&state, three, 3000000, &settings));
    assert(settings.rate_hz == 15 && settings.speed == JOY_SPEED_FAST);

    state = (gesture_state_t){0};
    settings.led_enabled = true;
    uint8_t led_pair = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2);
    uint32_t special_hold = JOY_SPECIAL_HOLD_MS * 1000u;
    assert(!joystick_gesture_step(&state, led_pair, 0, &settings));
    assert(!joystick_gesture_step(&state, led_pair, special_hold - 1, &settings));
    assert(joystick_gesture_step(&state, led_pair, special_hold, &settings));
    assert(!settings.led_enabled);
    assert(!joystick_gesture_step(&state, led_pair, special_hold + 1, &settings));
    assert(!joystick_gesture_step(&state, 0, special_hold + 2, &settings));
    assert(!joystick_gesture_step(&state, led_pair, special_hold + 3, &settings));
    assert(joystick_gesture_step(&state, led_pair,
                                 special_hold + 3 + special_hold, &settings));
    assert(settings.led_enabled);
}

static void test_factory_reset_gesture(void) {
    factory_reset_state_t state = {0};
    uint32_t hold = JOY_SPECIAL_HOLD_MS * 1000u;
    assert(!factory_reset_step(&state, true, 0));
    assert(!factory_reset_step(&state, true, hold - 1));
    assert(factory_reset_step(&state, true, hold));
    assert(!factory_reset_step(&state, true, hold + 1));
    assert(!factory_reset_step(&state, false, hold + 2));
    assert(!factory_reset_step(&state, true, hold + 3));
    assert(factory_reset_step(&state, true, hold + 3 + hold));
}

static void test_settings_persistence(void) {
    joystick_settings_t defaults;
    joystick_settings_defaults(&defaults);
    assert(defaults.speed == JOY_SPEED_FAST && defaults.rate_hz == 15 &&
           defaults.led_enabled);
    joystick_settings_record_t invalid = {0}, first =
        joystick_settings_record_make(&(joystick_settings_t){JOY_SPEED_SLOW, 25, true}, 4),
        second = joystick_settings_record_make(&(joystick_settings_t){JOY_SPEED_FAST, 10, false}, 5);
    joystick_settings_t loaded; unsigned slot;
    assert(!joystick_settings_load_records(&invalid, &invalid, &loaded, &slot));
    assert(loaded.speed == JOY_SPEED_FAST && loaded.rate_hz == 15 && loaded.led_enabled);
    joystick_settings_record_t old_version =
        joystick_settings_record_make(&(joystick_settings_t){JOY_SPEED_SLOW, 1, true}, 6);
    old_version.version = 1;
    assert(!joystick_settings_load_records(&old_version, &invalid, &loaded, &slot));
    assert(loaded.speed == JOY_SPEED_FAST && loaded.rate_hz == 15 && loaded.led_enabled);
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 1 && loaded.speed == JOY_SPEED_FAST && loaded.rate_hz == 10 &&
           !loaded.led_enabled);
    second.crc32 ^= 1u;
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 0 && loaded.speed == JOY_SPEED_SLOW && loaded.rate_hz == 25 &&
           loaded.led_enabled);
}

static void test_update_shortcut(void) {
    update_shortcut_t state = {0};
    assert(!update_shortcut_step(&state, true, 1000));
    assert(!update_shortcut_step(&state, false, 1000000));
    assert(!update_shortcut_step(&state, true, 1020000)); /* Grace preserves hold. */
    assert(!update_shortcut_step(&state, true, 3000999));
    assert(update_shortcut_step(&state, true, 3001000));

    state = (update_shortcut_t){0};
    assert(!update_shortcut_step(&state, true, 0));
    assert(!update_shortcut_step(&state, false, 1000000));
    assert(!update_shortcut_step(&state, false, 1030000));
    assert(!update_shortcut_step(&state, true, 1030001));
    assert(!update_shortcut_step(&state, true, 3030000));
    assert(update_shortcut_step(&state, true, 4030001));
}

static void test_report_deadline(void) {
    uint32_t next = 1000;
    assert(!joystick_report_due(&next, 999, JOY_FAST_REPORT_INTERVAL_US));
    assert(joystick_report_due(&next, 1000, JOY_FAST_REPORT_INTERVAL_US) && next == 2000);
    assert(joystick_report_due(&next, 4500, JOY_FAST_REPORT_INTERVAL_US) && next == 5000);
    next = 80000;
    assert(!joystick_report_due(&next, 79999, JOY_SLOW_REPORT_INTERVAL_US));
    assert(joystick_report_due(&next, 80000, JOY_SLOW_REPORT_INTERVAL_US) && next == 160000);
    next = UINT32_MAX - 499u;
    assert(joystick_report_due(&next, UINT32_MAX - 499u, JOY_FAST_REPORT_INTERVAL_US));
    assert(next == 500u);
    assert(!joystick_report_due(&next, 499u, JOY_FAST_REPORT_INTERVAL_US));
    assert(joystick_report_due(&next, 500u, JOY_FAST_REPORT_INTERVAL_US));
}

static void test_report_intervals(void) {
    assert(joystick_report_interval_us(JOY_SPEED_FAST, false) ==
           JOY_FAST_REPORT_INTERVAL_US);
    assert(joystick_report_interval_us(JOY_SPEED_FAST, true) ==
           JOY_FAST_REPORT_INTERVAL_US);
    assert(joystick_report_interval_us(JOY_SPEED_SLOW, false) ==
           JOY_SLOW_REPORT_INTERVAL_US);
    assert(joystick_report_interval_us(JOY_SPEED_SLOW, true) ==
           JOY_FAST_REPORT_INTERVAL_US);
}

int main(void) {
    test_gpio_snapshot();
    test_debounce();
    test_report_mapping();
    test_autofire();
    test_direct_activity();
    test_gestures();
    test_factory_reset_gesture();
    test_settings_persistence();
    test_update_shortcut();
    test_report_deadline();
    test_report_intervals();
    puts("joystick logic tests passed");
    return 0;
}
