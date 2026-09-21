#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "joystick.h"
#include "settings.h"

#define PRESSED(input) (1u << (input))

static void test_defaults_and_axis_reports(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    assert(profile->button[2].type == INI_BIND_GAMEPAD &&
           profile->button[2].value == 1 && profile->button[2].autofire);
    assert(settings.profiles[1].button[2].type == INI_BIND_GAMEPAD &&
           settings.profiles[1].button[2].value == 3 &&
           settings.profiles[1].button[2].autofire);
    autofire_state_t state = {0};
    joystick_report_t report = joystick_make_report(&state, PRESSED(INPUT_UP), profile, false);
    assert(report.x == 0 && report.y == -127 && report.buttons == 0);
    report = joystick_make_report(&state, PRESSED(INPUT_BIG_FIRE_1), profile, false);
    assert(report.buttons == 1);

    profile = &settings.profiles[1];
    profile->direction[INPUT_UP] = (ini_binding_t){INI_BIND_AXIS, INPUT_DOWN, 0, 0, 0};
    profile->direction[INPUT_DOWN] = (ini_binding_t){INI_BIND_AXIS, INPUT_UP, 0, 0, 0};
    report = joystick_make_report(&state, PRESSED(INPUT_UP), profile, false);
    assert(report.y == 127);
    report = joystick_make_report(&state, PRESSED(INPUT_UP) | PRESSED(INPUT_DOWN), profile, false);
    assert(report.y == 0);
}

static void test_unified_keyboard_bindings(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    profile->button[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 0, 0};
    profile->button[1] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A + 1, 0x05, 0, 0};
    profile->direction[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A + 22, 0, 0, 0};
    autofire_state_t state = {0};
    joystick_keyboard_report_t report = joystick_make_keyboard_report(
        &state, PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2) |
        PRESSED(INPUT_UP), profile, false);
    assert(report.modifier == (0x02 | 0x05));
    assert(report.keycodes[0] == 0x1a && report.keycodes[1] == 0x04 &&
           report.keycodes[2] == 0x05);
}

static void test_autofire_all_input_types(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    profile->button[0].autofire = 1;
    profile->direction[0].autofire = 1;
    autofire_state_t state = {0};
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_UP);
    assert(joystick_autofire_enabled(input, profile));
    autofire_state_update(&state, input, profile, 1000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    autofire_state_update(&state, input, profile, 1000 + half);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);
    profile->button[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 1, 0};
    autofire_state_init(&state);
    autofire_state_update(&state, PRESSED(INPUT_BIG_FIRE_1), profile, 2000);
    assert(joystick_make_keyboard_report(&state, PRESSED(INPUT_BIG_FIRE_1), profile, false).modifier == 0x02);
}

static void test_delayed_autofire(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    profile->button[0].autofire = 1;
    profile->button[0].autofire_delay_ms = 100;
    autofire_state_t state = {0};
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1);
    autofire_state_update(&state, input, profile, 1000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, 1000 + 99999);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, 101000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, 101000 + 500000 / JOY_AUTOFIRE_DEFAULT_HZ);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);
}

static void test_profile_gesture(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    gesture_state_t gesture = {0};
    uint8_t input = PRESSED(INPUT_SMALL_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_2) |
                    PRESSED(INPUT_LEFT);
    uint32_t delay = JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u;
    assert(!joystick_gesture_step(&gesture, input, 0, &settings));
    assert(joystick_gesture_step(&gesture, input, delay, &settings));
    assert(settings.active_profile == 2 && gesture.suppress_output);
}

static void test_factory_reset_scope(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    settings.profiles[1].button[0].value = INI_CODE_A;
    settings.profiles[2].button[0].value = INI_CODE_A + 1;
    settings.profiles[3].button[0].value = INI_CODE_A + 2;
    joystick_settings_reset_profile(&settings, 0);
    joystick_settings_select_profile(&settings, 0);
    assert(settings.active_profile == 0);
    assert(settings.profiles[0].button[0].type == INI_BIND_GAMEPAD &&
           settings.profiles[0].button[0].value == 1);
    assert(settings.profiles[0].button[2].type == INI_BIND_GAMEPAD &&
           settings.profiles[0].button[2].value == 1 &&
           settings.profiles[0].button[2].autofire);
    assert(settings.profiles[1].button[0].value == INI_CODE_A);
    assert(settings.profiles[2].button[0].value == INI_CODE_A + 1);
    assert(settings.profiles[3].button[0].value == INI_CODE_A + 2);
}

static void test_settings_persistence(void) {
    joystick_settings_t first_settings, second_settings, loaded;
    joystick_settings_defaults(&first_settings);
    joystick_settings_defaults(&second_settings);
    first_settings.profiles[2].button[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 0, 0};
    second_settings.active_profile = 3;
    second_settings.profiles[3].direction[0] = (ini_binding_t){INI_BIND_AXIS, INPUT_DOWN, 0, 1, 0};
    joystick_settings_record_t first = joystick_settings_record_make(&first_settings, 4);
    joystick_settings_record_t second = joystick_settings_record_make(&second_settings, 5);
    unsigned slot;
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 1 && joystick_settings_equal(&loaded, &second_settings));
    second.crc32 ^= 1u;
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 0 && joystick_settings_equal(&loaded, &first_settings));
}

int test_joystick_main(void) {
    test_defaults_and_axis_reports();
    test_unified_keyboard_bindings();
    test_autofire_all_input_types();
    test_delayed_autofire();
    test_profile_gesture();
    test_factory_reset_scope();
    test_settings_persistence();
    puts("joystick logic tests passed");
    return 0;
}
