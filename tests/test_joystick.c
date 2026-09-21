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

static void test_boot_mode_gesture(void) {
    const uint32_t config_us = JOY_SPECIAL_HOLD_MS * 1000u;
    const uint32_t firmware_us =
        (JOY_SPECIAL_HOLD_MS + JOY_CONFIG_MODE_HOLD_MS) * 1000u;
    boot_mode_state_t state = {0};

    assert(boot_mode_step(&state, true, 1000) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, 1000 + config_us - 1) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, 1000 + config_us) == BOOT_MODE_NONE);

    assert(boot_mode_step(&state, true, 2000) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, 2000 + config_us) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, 2000 + config_us + 1) == BOOT_MODE_CONFIG);

    assert(boot_mode_step(&state, true, 3000) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, 3000 + firmware_us) == BOOT_MODE_FIRMWARE);
    assert(boot_mode_step(&state, true, 3000 + firmware_us + 1000) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, false, 3000 + firmware_us + 2000) == BOOT_MODE_NONE);

    assert(boot_mode_step(&state, true, 4000) == BOOT_MODE_NONE);
    assert(boot_mode_step(&state, true, 4000 + firmware_us) == BOOT_MODE_FIRMWARE);
}

static void test_defaults_and_axis_reports(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    assert(profile->button[0].type == INI_BIND_GAMEPAD &&
           profile->button[0].value == 1 && !profile->button[0].autofire &&
           profile->button[0].autofire_delay_ms == 0 &&
           settings.autofire_hz[0][JOY_DIRECTION_COUNT] == 0);
    assert(profile->button[1].type == INI_BIND_KEYBOARD &&
           profile->button[1].value == INI_CODE_SPACE &&
           !profile->button[1].autofire);
    assert(profile->button[2].type == INI_BIND_GAMEPAD &&
           profile->button[2].value == 1 && profile->button[2].autofire &&
           settings.autofire_hz[0][JOY_DIRECTION_COUNT + 2] == 0);
    assert(profile->button[3].type == INI_BIND_GAMEPAD &&
           profile->button[3].value == 1 && profile->button[3].autofire &&
           settings.autofire_hz[0][JOY_DIRECTION_COUNT + 3] == 5);
    /* BLUE is profile slot 2 because profile storage follows the INI names
     * RED, GREEN, BLUE, YELLOW rather than the selection order. */
    assert(settings.profiles[2].button[0].type == INI_BIND_GAMEPAD &&
           settings.profiles[2].button[0].value == 1 &&
           !settings.profiles[2].button[0].autofire);
    assert(settings.profiles[2].button[1].type == INI_BIND_GAMEPAD &&
           settings.profiles[2].button[1].value == 2 &&
           !settings.profiles[2].button[1].autofire);
    assert(settings.profiles[2].button[2].type == INI_BIND_GAMEPAD &&
           settings.profiles[2].button[2].value == 1 &&
           settings.profiles[2].button[2].autofire &&
           settings.profiles[2].button[2].autofire_delay_ms == 0 &&
           settings.autofire_hz[2][JOY_DIRECTION_COUNT + 2] == 0);
    assert(settings.profiles[2].button[3].type == INI_BIND_GAMEPAD &&
           settings.profiles[2].button[3].value == 2 &&
           settings.profiles[2].button[3].autofire &&
           settings.profiles[2].button[3].autofire_delay_ms == 0 &&
           settings.autofire_hz[2][JOY_DIRECTION_COUNT + 3] == 0);
    assert(settings.profiles[3].button[0].type == INI_BIND_GAMEPAD &&
           settings.profiles[3].button[0].value == 1 &&
           settings.profiles[3].button[0].autofire &&
           settings.profiles[3].button[0].autofire_delay_ms == 500 &&
           settings.autofire_hz[3][JOY_DIRECTION_COUNT] == 25);
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
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_BIG_FIRE_2) |
                    PRESSED(INPUT_UP);
    autofire_state_update(&state, input, profile, NULL, 1000);
    joystick_keyboard_report_t report = joystick_make_keyboard_report(
        &state, input, profile, false);
    assert(report.modifier == (0x02 | 0x05));
    assert(report.keycodes[0] == 0x1a && report.keycodes[1] == 0x04 &&
           report.keycodes[2] == 0x05);
}

static void test_keyboard_tap(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    profile->button[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_SPACE, 0, 0, 0};
    autofire_state_t state = {0};
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1);
    autofire_state_update(&state, input, profile, NULL, 1000);
    joystick_keyboard_report_t report = joystick_make_keyboard_report(
        &state, input, profile, false);
    assert(report.keycodes[0] == 0x2c);
    autofire_state_update(&state, input, profile, NULL, 2000);
    report = joystick_make_keyboard_report(&state, input, profile, false);
    assert(report.keycodes[0] == 0);
    autofire_state_update(&state, 0, profile, NULL, 3000);
    report = joystick_make_keyboard_report(&state, 0, profile, false);
    assert(report.keycodes[0] == 0);
}

static void test_autofire_all_input_types(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[0];
    profile->button[0].autofire = 1;
    profile->button[0].autofire_delay_ms = 0;
    profile->direction[0].autofire = 1;
    autofire_state_t state = {0};
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1) | PRESSED(INPUT_UP);
    assert(joystick_autofire_enabled(input, profile));
    autofire_state_update(&state, input, profile, NULL, 1000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    uint32_t half = 500000u / JOY_AUTOFIRE_DEFAULT_HZ;
    autofire_state_update(&state, input, profile, NULL, 1000 + half);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);
    profile->button[0] = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 1, 0};
    autofire_state_init(&state);
    autofire_state_update(&state, PRESSED(INPUT_BIG_FIRE_1), profile, NULL, 2000);
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
    autofire_state_update(&state, input, profile, NULL, 1000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, NULL, 1000 + 99999);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, NULL, 101000);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, NULL,
                          101000 + 500000 / JOY_AUTOFIRE_DEFAULT_HZ);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);
}

static void test_factory_profile_autofire(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    autofire_state_t state = {0};

    /* YELLOW button 1: delayed fixed-rate autofire. */
    joystick_profile_t *profile = &settings.profiles[3];
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1);
    autofire_state_update(&state, input, profile, settings.autofire_hz[3], 1000);
    assert(!state.active);
    autofire_state_update(&state, input, profile, settings.autofire_hz[3], 501000);
    assert(state.active && state.pulse);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, settings.autofire_hz[3], 521000);
    assert(!state.pulse);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);

    /* BLUE button 3: immediate adjustable autofire. */
    profile = &settings.profiles[2];
    state = (autofire_state_t){ .hz = settings.rate_hz };
    input = PRESSED(INPUT_SMALL_FIRE_1);
    autofire_state_update(&state, input, profile, settings.autofire_hz[2], 1000);
    assert(state.active && state.pulse);
    autofire_state_update(&state, input, profile, settings.autofire_hz[2],
                          1000 + 500000u / settings.rate_hz);
    assert(!state.pulse);

    /* GREEN button 4: immediate fixed-rate autofire. */
    profile = &settings.profiles[1];
    state = (autofire_state_t){ .hz = settings.rate_hz };
    input = PRESSED(INPUT_SMALL_FIRE_2);
    autofire_state_update(&state, input, profile, settings.autofire_hz[1], 1000);
    assert(state.active && state.pulse);
    autofire_state_update(&state, input, profile, settings.autofire_hz[1], 21000);
    assert(!state.pulse);

    /* RED button 4: immediate fixed-rate autofire. */
    profile = &settings.profiles[0];
    state = (autofire_state_t){ .hz = settings.rate_hz };
    input = PRESSED(INPUT_SMALL_FIRE_2);
    autofire_state_update(&state, input, profile, settings.autofire_hz[0], 1000);
    assert(state.active && state.pulse);
    assert(joystick_make_report(&state, input, profile, false).buttons == 1);
    autofire_state_update(&state, input, profile, settings.autofire_hz[0], 121000);
    assert(!state.pulse);
    assert(joystick_make_report(&state, input, profile, false).buttons == 0);

    /* RED button 3: immediate adjustable-rate autofire. */
    profile = &settings.profiles[0];
    state = (autofire_state_t){ .hz = settings.rate_hz };
    input = PRESSED(INPUT_SMALL_FIRE_1);
    autofire_state_update(&state, input, profile, settings.autofire_hz[0], 1000);
    assert(state.active && state.pulse);
    autofire_state_update(&state, input, profile, settings.autofire_hz[0],
                          1000 + 500000u / settings.rate_hz);
    assert(!state.pulse);
}

static void test_shared_autofire_latest_pressed_fallback(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[JOY_PROFILE_RED];
    uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT];
    memcpy(fixed_rates, settings.autofire_hz[JOY_PROFILE_RED], sizeof(fixed_rates));
    autofire_state_t state;
    autofire_state_init(&state);

    uint8_t small_1 = PRESSED(INPUT_SMALL_FIRE_1);
    uint8_t small_2 = PRESSED(INPUT_SMALL_FIRE_2);
    uint8_t both = small_1 | small_2;

    /* Small Fire 2 is pressed last, so its fixed 5 Hz clock owns JOY1. */
    autofire_state_update(&state, small_1, profile, fixed_rates, 1000);
    autofire_state_update(&state, both, profile, fixed_rates, 2000);
    assert(joystick_make_report(&state, both, profile, false).buttons == 1);
    autofire_state_update(&state, both, profile, fixed_rates, 102000);
    assert(joystick_make_report(&state, both, profile, false).buttons == 0);

    /* Releasing Small Fire 2 restores Small Fire 1's adjustable autofire. */
    autofire_state_update(&state, small_1, profile, fixed_rates, 103000);
    assert(joystick_make_report(&state, small_1, profile, false).buttons == 1);
    autofire_state_update(&state, small_1, profile, fixed_rates, 128000);
    assert(joystick_make_report(&state, small_1, profile, false).buttons == 0);

    /* In the reverse order, releasing Small Fire 1 restores fixed autofire. */
    autofire_state_init(&state);
    autofire_state_update(&state, small_2, profile, fixed_rates, 200000);
    autofire_state_update(&state, both, profile, fixed_rates, 201000);
    autofire_state_update(&state, both, profile, fixed_rates, 301000);
    assert(joystick_make_report(&state, both, profile, false).buttons == 1);
    autofire_state_update(&state, small_2, profile, fixed_rates, 302000);
    assert(joystick_make_report(&state, small_2, profile, false).buttons == 1);
    autofire_state_update(&state, small_2, profile, fixed_rates, 402000);
    assert(joystick_make_report(&state, small_2, profile, false).buttons == 0);
}

static void test_runtime_step_autofire_transitions(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    joystick_profile_t *profile = &settings.profiles[3];
    autofire_state_t state;
    autofire_state_init(&state);
    uint8_t input = PRESSED(INPUT_BIG_FIRE_1);

    /* This follows the same single-sample operation used by main(). */
    joystick_runtime_output_t output = joystick_runtime_step(
        &state, input, profile, settings.autofire_hz[3], settings.speed,
        true, false, 1000);
    assert(!output.autofire_held);
    assert(output.joystick.buttons == 1);
    assert(output.led_active);

    output = joystick_runtime_step(
        &state, input, profile, settings.autofire_hz[3], settings.speed,
        true, false, 501000);
    assert(output.autofire_held && output.joystick.buttons == 1);
    assert(output.led_active);

    output = joystick_runtime_step(
        &state, input, profile, settings.autofire_hz[3], settings.speed,
        true, false, 526000);
    assert(output.autofire_held && output.joystick.buttons == 0);
    assert(!output.led_active);

    output = joystick_runtime_step(
        &state, 0, profile, settings.autofire_hz[3], settings.speed,
        false, false, 527000);
    assert(!output.autofire_held && output.joystick.buttons == 0);
    assert(!output.led_active);
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
    assert(settings.active_profile == 1 && gesture.suppress_output);

    settings.active_profile = 0;
    gesture = (gesture_state_t){0};
    input = PRESSED(INPUT_SMALL_FIRE_1) | PRESSED(INPUT_SMALL_FIRE_2) |
            PRESSED(INPUT_DOWN);
    assert(!joystick_gesture_step(&gesture, input, 0, &settings));
    assert(joystick_gesture_step(&gesture, input, delay, &settings));
    assert(settings.active_profile == 2 && gesture.suppress_output);
}

static void test_rate_adjustment_led(void) {
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    gesture_state_t gesture = {0};
    uint8_t decrease = PRESSED(INPUT_BIG_FIRE_1) |
                       PRESSED(INPUT_SMALL_FIRE_1);
    uint32_t activation = JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u;

    assert(!joystick_gesture_step(&gesture, decrease, 0, &settings));
    assert(!gesture.rate_adjust_active);
    assert(!joystick_rate_adjustment_led_step(&gesture, settings.rate_hz, 0));

    assert(joystick_gesture_step(&gesture, decrease, activation, &settings));
    assert(gesture.rate_adjust_active);
    assert(joystick_rate_adjustment_led_step(&gesture, settings.rate_hz,
                                             activation));
    uint32_t half_period = 500000u / settings.rate_hz;
    assert(!joystick_rate_adjustment_led_step(&gesture, settings.rate_hz,
                                              activation + half_period));
    assert(joystick_rate_adjustment_led_step(&gesture, settings.rate_hz,
                                             activation + 2u * half_period));

    joystick_gesture_step(&gesture, 0, activation + 3u * half_period, &settings);
    assert(!gesture.rate_adjust_active);
    assert(!joystick_rate_adjustment_led_step(&gesture, settings.rate_hz,
                                              activation + 3u * half_period));
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

    /* The exact bare-autofire form used in the RED profile must survive the
     * same record round trip as a normal saved configuration. */
    first_settings.profiles[0].button[0].autofire = 1;
    first_settings.profiles[0].button[0].autofire_delay_ms = 0;
    first_settings.autofire_hz[0][JOY_DIRECTION_COUNT] = 0;
    first = joystick_settings_record_make(&first_settings, 6);
    assert(joystick_settings_record_valid(&first));
    assert(joystick_settings_load_records(&first, &second, &loaded, &slot));
    assert(slot == 0 && loaded.profiles[0].button[0].autofire &&
           loaded.profiles[0].button[0].autofire_delay_ms == 0 &&
           loaded.autofire_hz[0][JOY_DIRECTION_COUNT] == 0);
}

int test_joystick_main(void) {
    test_boot_mode_gesture();
    test_defaults_and_axis_reports();
    test_unified_keyboard_bindings();
    test_keyboard_tap();
    test_autofire_all_input_types();
    test_delayed_autofire();
    test_factory_profile_autofire();
    test_shared_autofire_latest_pressed_fallback();
    test_runtime_step_autofire_transitions();
    test_profile_gesture();
    test_rate_adjustment_led();
    test_factory_reset_scope();
    test_settings_persistence();
    puts("joystick logic tests passed");
    return 0;
}
