#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ini_config.h"
#include "joystick.h"
#include "settings.h"

#define PRESSED(input) (1u << (input))

static joystick_settings_t default_settings(void) {
    joystick_settings_t s;
    joystick_settings_defaults(&s);
    return s;
}

/* Four gamepad buttons, no autofire: independent of the factory defaults. */
static joystick_settings_t plain_settings(void) {
    joystick_settings_t s;
    joystick_settings_defaults(&s);
    s.button_code[0] = INI_CODE_JOY1;
    s.button_code[1] = INI_CODE_JOY2;
    s.button_code[2] = INI_CODE_JOY3;
    s.button_code[3] = INI_CODE_JOY4;
    s.autofire_mask = 0;
    return s;
}

/* Mirror main(): advance the shared autofire timebase, then build a report. */
static joystick_report_t auto_report(autofire_state_t *state, uint8_t inputs,
                                     const joystick_settings_t *s,
                                     uint32_t now, bool suppress) {
    autofire_state_update(state, joystick_autofire_enabled(inputs, s), now);
    return joystick_make_report(state, inputs, s, suppress);
}

static void test_gpio_snapshot(void) {
    uint32_t levels = 0xffffffffu & ~(1u << JOY_GPIO_UP) &
                      ~(1u << JOY_GPIO_SMALL_FIRE_2);
    assert(joystick_gpio_snapshot(levels) ==
           (PRESSED(INPUT_UP) | PRESSED(INPUT_SMALL_FIRE_2)));
    assert(joystick_gpio_pressed(PRESSED(INPUT_BIG_FIRE_1), JOY_GPIO_BIG_FIRE_1));
    assert(!joystick_gpio_pressed(PRESSED(INPUT_BIG_FIRE_1), JOY_GPIO_BIG_FIRE_2));
    assert(JOY_UPDATE_GPIO_A == JOY_GPIO_SMALL_FIRE_1);
    assert(JOY_UPDATE_GPIO_B == JOY_GPIO_SMALL_FIRE_2);
    assert(joystick_fire_input(0) == INPUT_BIG_FIRE_1);
    assert(joystick_fire_input(1) == INPUT_BIG_FIRE_2);
    assert(joystick_fire_input(2) == INPUT_SMALL_FIRE_1);
    assert(joystick_fire_input(3) == INPUT_SMALL_FIRE_2);
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

static void test_factory_defaults(void) {
    joystick_settings_t s = default_settings();
    /* Original shipping mapping: B1->Button 1, B2->Button 2, S1->autofire
     * Button 1, S2->Button 3. */
    assert(s.button_code[0] == INI_CODE_JOY1 && s.button_code[1] == INI_CODE_JOY2);
    assert(s.button_code[2] == INI_CODE_JOY1 && s.button_code[3] == INI_CODE_JOY3);
    assert(s.autofire_mask == (1u << 2));
    assert(joystick_autofire_enabled(PRESSED(INPUT_SMALL_FIRE_1), &s) == true);
    assert(joystick_autofire_enabled(PRESSED(INPUT_SMALL_FIRE_2), &s) == false);
    assert(joystick_autofire_enabled(PRESSED(INPUT_BIG_FIRE_1), &s) == false);
    assert(joystick_autofire_enabled(0, &s) == false);

    /* Revving the shared pulse, holding only Small Fire 1 presses Button 1. */
    autofire_state_t state;
    autofire_state_init(&state);
    uint8_t trigger = PRESSED(INPUT_SMALL_FIRE_1);
    uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    autofire_state_update(&state, joystick_autofire_enabled(trigger, &s), 1000);
    assert(joystick_make_report(&state, trigger, &s, false).buttons == 1);
    autofire_state_update(&state, joystick_autofire_enabled(trigger, &s), 1000 + half);
    assert(joystick_make_report(&state, trigger, &s, false).buttons == 0);
}

static void test_report_mapping(void) {
    autofire_state_t state = {0};
    joystick_settings_t s = plain_settings();
    uint8_t inputs = PRESSED(INPUT_UP) | PRESSED(INPUT_LEFT) |
                     PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2) |
                     PRESSED(INPUT_SMALL_FIRE_2);
    joystick_report_t report = joystick_make_report(&state, inputs, &s, false);
    assert(sizeof(report) == 3);
    assert(report.x == -127 && report.y == -127 && report.buttons == 0x0b);
    report = joystick_make_report(&state, inputs | PRESSED(INPUT_DOWN) |
                                  PRESSED(INPUT_RIGHT), &s, false);
    assert(report.x == 0 && report.y == 0 && report.buttons == 0x0b);
    report = joystick_make_report(&state, 0, &s, false);
    assert(report.x == 0 && report.y == 0 && report.buttons == 0);
    uint8_t const *bytes = (uint8_t const *)&report;
    assert(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0);

    s.button_code[0] = INI_CODE_NONE;
    assert(joystick_make_report(&state, PRESSED(INPUT_BIG_FIRE_1), &s, false).buttons == 0);
    assert(joystick_make_report(&state, PRESSED(INPUT_BIG_FIRE_1), &s, true).buttons == 0);
}

static void test_keyboard_report(void) {
    autofire_state_t state = {0};
    joystick_settings_t s = plain_settings();
    joystick_keyboard_report_t k;

    /* All JOY codes are gamepad buttons: no keyboard output. */
    k = joystick_make_keyboard_report(&state, 0xff, &s, false);
    assert(k.modifier == 0 && k.keycodes[0] == 0 && k.reserved == 0);

    s.button_code[0] = INI_CODE_A;
    s.button_code[1] = INI_CODE_SHIFT;
    s.button_code[2] = INI_CODE_F1 + 4; /* F5 */
    s.button_code[3] = INI_CODE_NONE;
    uint8_t inputs = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2) |
                     PRESSED(INPUT_SMALL_FIRE_1);
    k = joystick_make_keyboard_report(&state, inputs, &s, false);
    assert(k.modifier == 0x02 && k.keycodes[0] == 0x04 && k.keycodes[1] == 0x3e);
    assert(k.keycodes[2] == 0 && k.keycodes[3] == 0 && k.keycodes[4] == 0 &&
           k.keycodes[5] == 0);

    /* Suppressing fire output empties the keyboard report. */
    k = joystick_make_keyboard_report(&state, inputs, &s, true);
    assert(k.modifier == 0 && k.keycodes[0] == 0);

    /* Duplicate keys collapse to a single keycode. */
    s.button_code[1] = INI_CODE_A;
    k = joystick_make_keyboard_report(&state, inputs, &s, false);
    assert(k.modifier == 0 && k.keycodes[0] == 0x04 && k.keycodes[1] == 0x3e);
    assert(k.keycodes[2] == 0 && k.keycodes[3] == 0 && k.keycodes[4] == 0 &&
           k.keycodes[5] == 0);

    /* Modifier-only codes set the modifier with no keycode. */
    s.button_code[0] = INI_CODE_CTRL;
    k = joystick_make_keyboard_report(&state, PRESSED(INPUT_BIG_FIRE_1), &s, false);
    assert(k.modifier == 0x01 && k.keycodes[0] == 0);

    /* Autofiring a key is gated by the shared pulse. */
    autofire_state_t af;
    autofire_state_init(&af);
    s.button_code[2] = INI_CODE_A;
    s.autofire_mask = 1u << 2;
    uint8_t trigger = PRESSED(INPUT_SMALL_FIRE_1);
    uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    autofire_state_update(&af, joystick_autofire_enabled(trigger, &s), 1000);
    k = joystick_make_keyboard_report(&af, trigger, &s, false);
    assert(k.keycodes[0] == 0x04);
    autofire_state_update(&af, joystick_autofire_enabled(trigger, &s),
                          1000 + half);
    k = joystick_make_keyboard_report(&af, trigger, &s, false);
    assert(k.keycodes[0] == 0);
}

static void test_autofire(void) {
    autofire_state_t state;
    autofire_state_init(&state);
    joystick_settings_t s = plain_settings();
    s.autofire_mask = 1u << 2; /* Small Fire 1 autofires its JOY3 output. */
    const uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    uint8_t trigger = PRESSED(INPUT_SMALL_FIRE_1);
    assert(auto_report(&state, trigger, &s, 1000, false).buttons == 4);
    assert(auto_report(&state, trigger, &s, 1000 + half - 1, false).buttons == 4);
    assert(auto_report(&state, trigger, &s, 1000 + half, false).buttons == 0);
    assert(auto_report(&state, trigger | PRESSED(INPUT_BIG_FIRE_1),
                        &s, 1000 + half + 1, false).buttons == 1);
    assert(auto_report(&state, trigger | PRESSED(INPUT_BIG_FIRE_1),
                        &s, 1000 + half + 2, true).buttons == 0);
    assert(auto_report(&state, trigger, &s, 1000 + 3 * half, false).buttons == 0);
    assert(auto_report(&state, trigger, &s, 1000 + 4 * half, false).buttons == 4);
    assert(auto_report(&state, 0, &s, 1000 + 4 * half + 1, false).buttons == 0);
    assert(auto_report(&state, trigger, &s, 1000 + 4 * half + 2, false).buttons == 4);

    /* No autofire here: Small Fire 1 + Big Fire 2 both output directly. */
    autofire_state_init(&state);
    joystick_settings_t plain = plain_settings();
    uint8_t increase = trigger | PRESSED(INPUT_BIG_FIRE_2);
    assert(auto_report(&state, increase, &plain, 2000, false).buttons == 6);

    autofire_state_init(&state);
    state.hz = 15;
    const uint32_t half2 = 500000u / 15u;
    autofire_state_update(&state, true, 1000);
    assert(state.pulse);
    autofire_state_update(&state, true, 1000 + half2 - 1);
    assert(state.pulse);
    autofire_state_update(&state, true, 1000 + half2);
    assert(!state.pulse);
    autofire_state_update(&state, false, 1000 + half2 + 1);
    assert(!state.active && !state.pulse);

    /* Autofire takes priority over direct activity so the status LED follows
     * the same high/low pulse as the generated output. */
    assert(joystick_status_led_active(true, true, true));
    assert(!joystick_status_led_active(true, true, false));
    assert(joystick_status_led_active(true, false, false));
    assert(!joystick_status_led_active(false, false, true));
}

static void test_autofire_priority(void) {
    autofire_state_t state;
    autofire_state_init(&state);
    joystick_settings_t s = plain_settings();
    const uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;

    /* Big Fire 1 = JOY1 (normal), Small Fire 1 = JOY1:AUTOFIRE. */
    s.button_code[0] = INI_CODE_JOY1;
    s.button_code[2] = INI_CODE_JOY1;
    s.autofire_mask = 1u << 2;
    uint8_t both = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_1);

    /* Autofire wins: the gamepad button follows only the pulse while both are
     * held; the held normal button cannot keep it asserted. */
    assert(auto_report(&state, both, &s, 1000, false).buttons == 0x01);
    assert(auto_report(&state, both, &s, 1000 + half, false).buttons == 0);
    assert(auto_report(&state, both, &s, 1000 + 2 * half, false).buttons == 0x01);
    /* The normal button alone still presses when no autofire is held. */
    assert(auto_report(&state, PRESSED(INPUT_BIG_FIRE_1), &s,
                       1000 + 2 * half + 1, false).buttons == 0x01);

    /* Keyboard: SPACE (normal) + SPACE:AUTOFIRE. */
    autofire_state_init(&state);
    s.button_code[0] = INI_CODE_SPACE;
    s.button_code[1] = INI_CODE_JOY2;
    s.button_code[2] = INI_CODE_SPACE;
    s.button_code[3] = INI_CODE_JOY4;
    const uint8_t space = ini_config_keycode(INI_CODE_SPACE);
    uint8_t keys = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_1);
    autofire_state_update(&state, true, 2000);
    joystick_keyboard_report_t k =
        joystick_make_keyboard_report(&state, keys, &s, false);
    assert(k.keycodes[0] == space && k.keycodes[1] == 0);
    autofire_state_update(&state, true, 2000 + half);
    k = joystick_make_keyboard_report(&state, keys, &s, false);
    assert(k.keycodes[0] == 0);
    /* Normal-only SPACE still types. */
    autofire_state_init(&state);
    k = joystick_make_keyboard_report(&state, PRESSED(INPUT_BIG_FIRE_1), &s, false);
    assert(k.keycodes[0] == space);
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
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    settings.speed = JOY_SPEED_FAST;
    settings.rate_hz = 15;
    settings.button_code[0] = 1;
    settings.button_code[1] = 2;
    settings.button_code[2] = 3;
    settings.button_code[3] = 4;
    settings.autofire_mask = 0;
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
    assert(state.suppress_output);
    uint32_t repeat = JOY_AUTOFIRE_REPEAT_MS * 1000u;
    assert(!joystick_gesture_step(&state, down, gesture_delay + repeat - 1, &settings));
    assert(joystick_gesture_step(&state, down, gesture_delay + repeat, &settings));
    assert(settings.rate_hz == 13);
    assert(!joystick_gesture_step(&state, 0, gesture_delay + 100001, &settings));
    assert(!state.suppress_output);
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
    assert(state.suppress_output);
    assert(!joystick_gesture_step(&state, slow, 1, &settings));
    assert(!joystick_gesture_step(&state, 0, 2, &settings));
    assert(!state.suppress_output);
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

    /* Small-button pair plus a cardinal direction selects a color profile. */
    joystick_settings_defaults(&settings);
    state = (gesture_state_t){0};
    uint8_t pair_up = small_pair | PRESSED(INPUT_UP);
    assert(!joystick_gesture_step(&state, pair_up, 0, &settings));
    assert(!joystick_gesture_step(&state, pair_up, gesture_delay, &settings));
    assert(settings.active_profile == 0 && state.suppress_output);
    assert((state.release_mask & pair_up) == pair_up);
    state = (gesture_state_t){0};
    uint8_t pair_left = small_pair | PRESSED(INPUT_LEFT);
    assert(!joystick_gesture_step(&state, pair_left, 0, &settings));
    assert(joystick_gesture_step(&state, pair_left, gesture_delay, &settings));
    assert(settings.active_profile == 2);

    /* LED toggle at 3s, with output suppressed. */
    state = (gesture_state_t){0};
    settings.led_enabled = true;
    uint8_t led_pair = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2);
    uint32_t special_hold = JOY_SPECIAL_HOLD_MS * 1000u;
    assert(!joystick_gesture_step(&state, led_pair, 0, &settings));
    assert(!state.suppress_output);
    assert(!joystick_gesture_step(&state, led_pair, special_hold - 1, &settings));
    assert(joystick_gesture_step(&state, led_pair, special_hold, &settings));
    assert(!settings.led_enabled);
    assert(state.suppress_output);
    assert(!joystick_gesture_step(&state, 0, special_hold + 2, &settings));
    assert(!state.suppress_output);

    /* A fresh hold toggles LED back on. */
    assert(!joystick_gesture_step(&state, led_pair, special_hold + 3, &settings));
    assert(joystick_gesture_step(&state, led_pair, special_hold + 3 + special_hold,
                                 &settings));
    assert(settings.led_enabled);
    /* Holding past the toggle does not trigger again. */
    assert(!joystick_gesture_step(&state, led_pair,
                                  special_hold + 3 + special_hold + special_hold,
                                  &settings));
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
    assert(defaults.speed == JOY_SPEED_FAST && defaults.rate_hz == 20 &&
           defaults.led_enabled);
    assert(defaults.button_code[0] == INI_CODE_JOY1 &&
           defaults.button_code[1] == INI_CODE_JOY2 &&
           defaults.button_code[2] == INI_CODE_JOY1 &&
           defaults.button_code[3] == INI_CODE_JOY3 &&
           defaults.autofire_mask == (1u << 2));
    joystick_settings_record_t invalid = {0};
    joystick_settings_t first_settings, second_settings;
    joystick_settings_defaults(&first_settings);
    first_settings.speed = JOY_SPEED_SLOW;
    first_settings.rate_hz = 25;
    first_settings.button_code[0] = INI_CODE_A;
    first_settings.button_code[1] = INI_CODE_SPACE;
    first_settings.button_code[2] = INI_CODE_NONE;
    first_settings.button_code[3] = INI_CODE_F1 + 11;
    first_settings.autofire_mask = 0x5;
    joystick_settings_sync_active_profile(&first_settings);
    joystick_settings_defaults(&second_settings);
    second_settings.rate_hz = 10;
    second_settings.led_enabled = false;
    second_settings.button_code[0] = INI_CODE_JOY1;
    second_settings.button_code[1] = INI_CODE_SHIFT;
    second_settings.button_code[2] = INI_CODE_0;
    second_settings.button_code[3] = INI_CODE_ESC;
    joystick_settings_sync_active_profile(&second_settings);
    joystick_settings_record_t first =
        joystick_settings_record_make(&first_settings, 4),
        second = joystick_settings_record_make(&second_settings, 5);
    joystick_settings_t loaded; unsigned slot;
    assert(!joystick_settings_load_records(&invalid, &invalid, &loaded, &slot));
    assert(loaded.speed == JOY_SPEED_FAST && loaded.rate_hz == 20 && loaded.led_enabled);
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 1 && joystick_settings_equal(&loaded, &second_settings));
    second.crc32 ^= 1u;
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 0 && joystick_settings_equal(&loaded, &first_settings));

    /* An out-of-range button code invalidates a record. */
    joystick_settings_record_t bad_code = first;
    bad_code.crc32 = 0;
    bad_code.profile_button_code[0][0] = INI_CODE_COUNT;
    assert(!joystick_settings_record_valid(&bad_code));
    /* And an out-of-range autofire mask too. */
    bad_code = first;
    bad_code.crc32 = 0;
    bad_code.profile_autofire_mask[0] = 0x10;
    assert(!joystick_settings_record_valid(&bad_code));
    assert(!joystick_settings_equal(&first_settings, &second_settings));
}

static void test_factory_reset_profile_scope(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    settings.profiles[1][0] = INI_CODE_A;
    settings.profiles[2][0] = INI_CODE_A + 1;
    settings.profiles[3][0] = INI_CODE_A + 2;
    settings.profile_autofire_mask[1] = 0;
    settings.profile_autofire_mask[2] = 0;
    settings.profile_autofire_mask[3] = 0;
    joystick_settings_select_profile(&settings, 2);

    joystick_settings_t expected = settings;
    expected.speed = JOY_SPEED_FAST;
    expected.rate_hz = JOY_AUTOFIRE_DEFAULT_HZ;
    expected.led_enabled = true;
    joystick_settings_reset_profile(&settings, 0);
    joystick_settings_select_profile(&settings, 0);

    assert(settings.active_profile == 0);
    assert(settings.button_code[0] == INI_CODE_JOY1 &&
           settings.button_code[1] == INI_CODE_JOY2 &&
           settings.button_code[2] == INI_CODE_JOY1 &&
           settings.button_code[3] == INI_CODE_JOY3 &&
           settings.autofire_mask == (1u << 2));
    assert(settings.profiles[1][0] == expected.profiles[1][0]);
    assert(settings.profiles[2][0] == expected.profiles[2][0]);
    assert(settings.profiles[3][0] == expected.profiles[3][0]);
}

static void test_boot_mode(void) {
    boot_mode_state_t state = {0};
    uint32_t special_hold = JOY_SPECIAL_HOLD_MS * 1000u;
    uint32_t firmware_hold = (JOY_SPECIAL_HOLD_MS + JOY_CONFIG_MODE_HOLD_MS) * 1000u;

    /* Release before 3s: nothing. */
    assert(boot_mode_step(&state, true, 0) == BOOT_MODE_NONE);
    assert(state.held && state.mode == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, 1000000) == BOOT_MODE_NONE);
    assert(!state.held && state.mode == BOOT_MODE_NONE);

    /* Hold to 3s, release: config mode. */
    assert(boot_mode_step(&state, true, 0) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, special_hold - 1) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, special_hold)
           == BOOT_MODE_NONE && state.mode == BOOT_MODE_CONFIG);
    assert(boot_mode_step(&state, false, special_hold + 1) == BOOT_MODE_CONFIG);
    assert(!state.held);

    /* A new hold: config selected at 3s, firmware at 6s; mode stays on release. */
    assert(boot_mode_step(&state, true, special_hold + 5) == BOOT_MODE_NONE);
    uint32_t press2 = special_hold + 5;
    assert(boot_mode_step(&state, true, press2 + special_hold) == BOOT_MODE_NONE
           && state.mode == BOOT_MODE_CONFIG);
    assert(boot_mode_step(&state, true, press2 + firmware_hold - 1) == BOOT_MODE_NONE
           && state.mode == BOOT_MODE_CONFIG);
    assert(boot_mode_step(&state, true, press2 + firmware_hold) == BOOT_MODE_NONE
           && state.mode == BOOT_MODE_FIRMWARE);
    assert(boot_mode_step(&state, true, press2 + firmware_hold + firmware_hold)
           == BOOT_MODE_NONE && state.mode == BOOT_MODE_FIRMWARE);
    assert(boot_mode_step(&state, false, press2 + firmware_hold + firmware_hold + 1)
           == BOOT_MODE_FIRMWARE);

    /* Exactly one action per hold; re-press without release stubs nothing. */
    state = (boot_mode_state_t){0};
    assert(boot_mode_step(&state, true, 0) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, firmware_hold) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, firmware_hold + 1) == BOOT_MODE_FIRMWARE);
    assert(boot_mode_step(&state, false, firmware_hold + 2) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, firmware_hold + 3) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, firmware_hold + 4) == BOOT_MODE_NONE);
}

static void test_config_mode_exit(void) {
    config_exit_state_t state = {0};
    uint32_t hold = JOY_SPECIAL_HOLD_MS * 1000u;

    assert(!config_mode_exit_step(&state, true, 0));
    assert(!config_mode_exit_step(&state, true, hold - 1));
    assert(config_mode_exit_step(&state, true, hold));
    assert(!config_mode_exit_step(&state, true, hold + 1));
    assert(!config_mode_exit_step(&state, false, hold + 2));

    /* A new hold is allowed only after release. */
    assert(!config_mode_exit_step(&state, true, hold + 3));
    assert(config_mode_exit_step(&state, true, hold + 3 + hold));
}

static void test_post_reboot_input_guard(void) {
    assert(joystick_post_reboot_guard_active(true, PRESSED(INPUT_SMALL_FIRE_1)));
    assert(joystick_post_reboot_guard_active(true, PRESSED(INPUT_UP)));
    assert(!joystick_post_reboot_guard_active(true, 0));
    assert(!joystick_post_reboot_guard_active(false, PRESSED(INPUT_BIG_FIRE_1)));
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

int test_joystick_main(void) {
    test_gpio_snapshot();
    test_debounce();
    test_factory_defaults();
    test_report_mapping();
    test_keyboard_report();
    test_autofire();
    test_autofire_priority();
    test_direct_activity();
    test_gestures();
    test_factory_reset_gesture();
    test_settings_persistence();
    test_factory_reset_profile_scope();
    test_boot_mode();
    test_config_mode_exit();
    test_post_reboot_input_guard();
    test_report_deadline();
    test_report_intervals();
    puts("joystick logic tests passed");
    return 0;
}
