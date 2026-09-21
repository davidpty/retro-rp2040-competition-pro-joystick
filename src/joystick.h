#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "ini_config.h"

typedef enum {
    INPUT_UP, INPUT_DOWN, INPUT_LEFT, INPUT_RIGHT,
    INPUT_BIG_FIRE_1, INPUT_BIG_FIRE_2, INPUT_SMALL_FIRE_1, INPUT_SMALL_FIRE_2,
    INPUT_COUNT
} input_id_t;

typedef struct {
    uint8_t stable;
    uint8_t candidate;
    uint32_t changed_at_us[INPUT_COUNT];
} input_filter_t;

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t buttons;
} joystick_report_t;

/* Standard 8-byte HID keyboard report: modifier, reserved, then six keycodes. */
typedef struct {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycodes[6];
} joystick_keyboard_report_t;

typedef struct {
    ini_binding_t direction[JOY_DIRECTION_COUNT];
    ini_binding_t button[JOY_BUTTON_COUNT];
} joystick_profile_t;

typedef struct {
    bool active;
    bool pulse;
    uint8_t pulse_mask;
    uint8_t ready_mask;
    uint8_t fixed_mask;
    uint8_t global_ready_mask;
    bool global_pulse;
    uint16_t global_hz;
    uint8_t started_mask;
    uint8_t previous_inputs;
    uint8_t keyboard_tap_mask;
    uint32_t last_toggle_us;
    uint32_t input_last_toggle_us[INPUT_COUNT];
    uint32_t input_started_at_us[INPUT_COUNT];
    uint16_t hz;
} autofire_state_t;

/* Result of one complete sampled-input update. Keeping these values together
 * prevents the main loop from using a pre-update autofire decision for one
 * output while using post-update state for another. */
typedef struct {
    joystick_report_t joystick;
    joystick_keyboard_report_t keyboard;
    uint32_t report_interval_us;
    bool autofire_held;
    bool led_active;
} joystick_runtime_output_t;

typedef struct {
    bool rate_adjust_held;
    bool rate_adjust_active;
    bool rate_decrease;
    bool mode_held;
    bool mode_triggered;
    bool mode_slow;
    bool led_held;
    bool led_triggered;
    bool profile_held;
    bool profile_triggered;
    uint8_t profile_index;
    bool suppress_output;
    uint32_t rate_next_us;
    uint32_t rate_start_us;
    uint32_t rate_led_last_toggle_us;
    uint32_t mode_start_us;
    uint32_t led_start_us;
    uint32_t profile_start_us;
    uint8_t release_mask;
    bool rate_led_pulse;
} gesture_state_t;

typedef enum {
    BOOT_MODE_NONE = 0,   /* No mode selected (released too early). */
    BOOT_MODE_CONFIG,     /* Release after JOY_SPECIAL_HOLD_MS: config drive. */
    BOOT_MODE_FIRMWARE    /* At 6s: firmware update (BOOTSEL) immediately. */
} boot_mode_action_t;

typedef struct {
    bool held;
    bool fired;
    uint8_t mode;          /* Live selection while held: BOOT_MODE_* value. */
    uint32_t hold_start_us;
} boot_mode_state_t;

typedef struct {
    bool held;
    bool triggered;
    uint32_t hold_start_us;
} config_exit_state_t;

typedef struct {
    bool held;
    bool triggered;
    uint32_t hold_start_us;
} factory_reset_state_t;

typedef enum {
    JOY_SPEED_FAST,
    JOY_SPEED_SLOW
} joystick_speed_t;

typedef struct {
    joystick_speed_t speed;
    uint8_t rate_hz;
    bool led_enabled;
    joystick_profile_t profiles[JOY_PROFILE_COUNT];
    uint8_t active_profile;
    uint16_t settings_token;
    uint8_t autofire_hz[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
} joystick_settings_t;

uint8_t joystick_gpio_snapshot(uint32_t gpio_levels);
void input_filter_init(input_filter_t *filter, uint8_t raw);
uint8_t input_filter_update(input_filter_t *filter, uint8_t raw, uint32_t now_us);
void autofire_state_init(autofire_state_t *state);
void autofire_state_update(autofire_state_t *state, uint8_t inputs,
                           const joystick_profile_t *profile,
                           const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT],
                           uint32_t now_us);
bool joystick_autofire_enabled(uint8_t inputs, const joystick_profile_t *profile);
uint32_t joystick_report_interval_us(joystick_speed_t speed, bool autofire_active);
joystick_report_t joystick_make_report(autofire_state_t *state, uint8_t inputs,
                                       const joystick_profile_t *profile,
                                       bool suppress_fire);
joystick_keyboard_report_t joystick_make_keyboard_report(autofire_state_t *state,
                                       uint8_t inputs,
                                       const joystick_profile_t *profile,
                                       bool suppress_fire);
joystick_runtime_output_t joystick_runtime_step(
    autofire_state_t *state, uint8_t inputs,
    const joystick_profile_t *profile,
    const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT],
    joystick_speed_t speed, bool direct_active, bool suppress_fire,
    uint32_t now_us);
bool joystick_direct_activity(uint8_t inputs, bool ignore_fire_buttons);
bool joystick_status_led_active(bool direct_active, bool autofire_held,
                                bool autofire_pulse);
bool joystick_post_reboot_guard_active(bool guard_active, uint8_t inputs);
bool joystick_gesture_step(gesture_state_t *state, uint8_t inputs,
                           uint32_t now_us, joystick_settings_t *settings);
bool joystick_rate_adjustment_led_step(gesture_state_t *state, uint8_t rate_hz,
                                       uint32_t now_us);
boot_mode_action_t boot_mode_step(boot_mode_state_t *state, bool both_pressed,
                                  uint32_t now_us);
bool config_mode_exit_step(config_exit_state_t *state, bool both_pressed,
                           uint32_t now_us);
bool factory_reset_step(factory_reset_state_t *state, bool all_pressed,
                        uint32_t now_us);
bool joystick_report_due(uint32_t *next_us, uint32_t now_us, uint32_t interval_us);
bool joystick_input_pressed(uint8_t inputs, input_id_t input);
uint8_t joystick_input_gpio(input_id_t input);
bool joystick_gpio_pressed(uint8_t inputs, uint8_t gpio);
input_id_t joystick_fire_input(unsigned fire_button);

#endif
