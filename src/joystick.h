#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <stdbool.h>
#include <stdint.h>

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

typedef struct {
    bool active;
    bool pulse;
    uint32_t last_toggle_us;
    uint16_t hz;
} autofire_state_t;

typedef struct {
    bool rate_adjust_held;
    bool rate_adjust_active;
    bool rate_decrease;
    bool mode_held;
    bool mode_triggered;
    bool mode_slow;
    bool led_held;
    bool led_triggered;
    uint32_t rate_next_us;
    uint32_t rate_start_us;
    uint32_t mode_start_us;
    uint32_t led_start_us;
    uint8_t release_mask;
} gesture_state_t;

typedef struct {
    bool held;
    bool release_pending;
    uint32_t hold_start_us;
    uint32_t release_start_us;
} update_shortcut_t;

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
} joystick_settings_t;

uint8_t joystick_gpio_snapshot(uint32_t gpio_levels);
void input_filter_init(input_filter_t *filter, uint8_t raw);
uint8_t input_filter_update(input_filter_t *filter, uint8_t raw, uint32_t now_us);
void autofire_state_init(autofire_state_t *state);
void autofire_state_update(autofire_state_t *state, bool enabled, uint32_t now_us);
uint32_t joystick_report_interval_us(joystick_speed_t speed, bool autofire_active);
joystick_report_t joystick_make_report(autofire_state_t *state, uint8_t inputs,
                                       uint32_t now_us,
                                       bool suppress_autofire_target_direct);
bool joystick_direct_activity(uint8_t inputs, bool ignore_fire_buttons);
bool joystick_gesture_step(gesture_state_t *state, uint8_t inputs,
                           uint32_t now_us, joystick_settings_t *settings);
bool update_shortcut_step(update_shortcut_t *state, bool both_pressed,
                          uint32_t now_us);
bool factory_reset_step(factory_reset_state_t *state, bool all_pressed,
                        uint32_t now_us);
bool joystick_report_due(uint32_t *next_us, uint32_t now_us, uint32_t interval_us);
bool joystick_input_pressed(uint8_t inputs, input_id_t input);
uint8_t joystick_input_gpio(input_id_t input);
bool joystick_gpio_pressed(uint8_t inputs, uint8_t gpio);

#endif
