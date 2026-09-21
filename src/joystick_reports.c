#include "joystick_reports.h"

#include <stddef.h>

#include "config.h"
#include "ini_config.h"
#include "joystick_autofire.h"
#include "joystick_input.h"
#include "joystick_gestures.h"

_Static_assert(sizeof(joystick_report_t) == 3, "HID report must be 3 bytes");
_Static_assert(offsetof(joystick_report_t, x) == 0 &&
               offsetof(joystick_report_t, y) == 1 &&
               offsetof(joystick_report_t, buttons) == 2,
               "HID report fields must match descriptor order");
_Static_assert(sizeof(joystick_keyboard_report_t) == 8,
               "HID keyboard report must be 8 bytes");
_Static_assert(JOY_FAST_REPORT_INTERVAL_US > 0 && JOY_SLOW_REPORT_INTERVAL_US > 0,
               "Report intervals must be positive");
_Static_assert(JOY_AUTOFIRE_MIN_HZ > 0 && JOY_AUTOFIRE_MIN_HZ <= JOY_AUTOFIRE_MAX_HZ &&
               JOY_AUTOFIRE_MAX_HZ <= 500,
               "Autofire frequency range must be between 1 and 500 Hz");
_Static_assert(JOY_AUTOFIRE_DEFAULT_HZ >= JOY_AUTOFIRE_MIN_HZ &&
               JOY_AUTOFIRE_DEFAULT_HZ <= JOY_AUTOFIRE_MAX_HZ,
               "Autofire default must be inside the configured frequency range");
_Static_assert(JOY_AUTOFIRE_REPEAT_MS > 0, "Autofire repeat interval must be positive");

static const ini_binding_t *input_binding(const joystick_profile_t *profile,
                                          unsigned input) {
    return input < JOY_DIRECTION_COUNT
        ? &profile->direction[input] : &profile->button[input - JOY_DIRECTION_COUNT];
}

void autofire_state_init(autofire_state_t *state) {
    *state = (autofire_state_t){ .hz = JOY_AUTOFIRE_DEFAULT_HZ };
}

void autofire_state_update(autofire_state_t *state, uint8_t inputs,
                           const joystick_profile_t *profile,
                           const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT],
                           uint32_t now_us) {
    uint8_t newly_pressed = inputs & (uint8_t)~state->previous_inputs;
    state->previous_inputs = inputs;
    state->keyboard_tap_mask = 0;
    uint8_t held_mask = 0;
    state->ready_mask = 0;
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        const ini_binding_t *binding = input_binding(profile, input);
        uint8_t mask = (uint8_t)(1u << input);
        if (newly_pressed & mask) state->press_order[input] = ++state->next_press_order;
        if ((newly_pressed & mask) && binding->type == INI_BIND_KEYBOARD &&
            !binding->autofire) state->keyboard_tap_mask |= mask;
        if (!binding->autofire || !(inputs & mask)) {
            state->started_mask &= (uint8_t)~mask;
            continue;
        }
        held_mask |= mask;
        if (!(state->started_mask & mask)) {
            state->started_mask |= mask;
            state->input_started_at_us[input] = now_us;
        }
        if ((uint32_t)(now_us - state->input_started_at_us[input]) >=
            (uint32_t)binding->autofire_delay_ms * 1000u) state->ready_mask |= mask;
    }
    state->started_mask &= held_mask;
    state->pulse_mask &= state->ready_mask;
    state->fixed_mask = 0;
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        uint8_t mask = (uint8_t)(1u << input);
        if ((state->ready_mask & mask) && fixed_rates && fixed_rates[input])
            state->fixed_mask |= mask;
    }
    state->pulse_mask &= state->fixed_mask;

    uint8_t global_ready = state->ready_mask & (uint8_t)~state->fixed_mask;
    state->global_hz = state->hz ? state->hz : JOY_AUTOFIRE_DEFAULT_HZ;
    if (!global_ready && state->fixed_mask &&
        (state->fixed_mask & (uint8_t)(state->fixed_mask - 1u)) == 0) {
        unsigned input = 0;
        while (!(state->fixed_mask & (1u << input))) ++input;
        state->global_hz = fixed_rates[input];
        state->fixed_mask = 0;
        global_ready = state->ready_mask;
    }
    if (!global_ready) {
        state->global_ready_mask = 0;
        state->global_pulse = false;
        state->last_toggle_us = now_us;
    } else if (!state->global_ready_mask) {
        state->global_ready_mask = global_ready;
        state->global_pulse = true;
        state->last_toggle_us = now_us;
    } else {
        uint32_t half_period_us = 500000u / state->global_hz;
        uint32_t periods = (uint32_t)(now_us - state->last_toggle_us) / half_period_us;
        if (periods & 1u) state->global_pulse = !state->global_pulse;
        if (periods) state->last_toggle_us += periods * half_period_us;
        state->global_ready_mask = global_ready;
    }

    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        uint8_t mask = (uint8_t)(1u << input);
        if (!(state->fixed_mask & mask)) continue;
        uint32_t hz = fixed_rates && fixed_rates[input] ? fixed_rates[input] : state->hz;
        if (!hz) hz = JOY_AUTOFIRE_DEFAULT_HZ;
        uint32_t half_period_us = 500000u / hz;
        if (!(state->pulse_mask & mask)) {
            state->pulse_mask |= mask;
            state->input_last_toggle_us[input] = now_us;
            continue;
        }
        uint32_t periods = (uint32_t)(now_us - state->input_last_toggle_us[input]) /
                           half_period_us;
        if (periods & 1u) state->pulse_mask ^= mask;
        if (periods) state->input_last_toggle_us[input] += periods * half_period_us;
    }
    state->active = state->ready_mask != 0;
    state->pulse = state->global_pulse || state->pulse_mask != 0;
}

static bool binding_equal(const ini_binding_t *a, const ini_binding_t *b) {
    return a->type == b->type && a->value == b->value && a->modifier == b->modifier;
}

static bool autofire_input_is_newest(const autofire_state_t *state,
                                     const joystick_profile_t *profile,
                                     unsigned input, uint8_t inputs) {
    const ini_binding_t *binding = input_binding(profile, input);
    if (!binding->autofire || !joystick_input_pressed(inputs, (input_id_t)input))
        return false;
    for (unsigned other = 0; other < INPUT_COUNT; ++other) {
        const ini_binding_t *candidate = input_binding(profile, other);
        if (other != input && candidate->autofire &&
            joystick_input_pressed(inputs, (input_id_t)other) &&
            binding_equal(candidate, binding) &&
            state->press_order[other] > state->press_order[input])
            return false;
    }
    return true;
}

static bool autofire_input_pulse(const autofire_state_t *state, unsigned input) {
    uint8_t mask = (uint8_t)(1u << input);
    return (state->fixed_mask & mask)
        ? (state->pulse_mask & mask) != 0
        : state->global_pulse;
}

static bool binding_effective(const autofire_state_t *state,
                              const joystick_profile_t *profile,
                              unsigned input, uint8_t inputs) {
    const ini_binding_t *binding = input_binding(profile, input);
    if (!joystick_input_pressed(inputs, (input_id_t)input)) return false;
    if (!(state->ready_mask & (1u << input))) return true;
    if (binding->autofire) {
        if (!autofire_input_is_newest(state, profile, input, inputs)) return false;
        return autofire_input_pulse(state, input);
    }
    for (unsigned other = 0; other < INPUT_COUNT; ++other) {
        const ini_binding_t *candidate = input_binding(profile, other);
        if (other != input && candidate->autofire &&
            joystick_input_pressed(inputs, (input_id_t)other) &&
            (state->ready_mask & (1u << other)) && binding_equal(candidate, binding))
            return false;
    }
    return true;
}

bool joystick_autofire_enabled(uint8_t inputs, const joystick_profile_t *profile) {
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        const ini_binding_t *binding = input_binding(profile, input);
        if (binding->autofire && joystick_input_pressed(inputs, (input_id_t)input))
            return true;
    }
    return false;
}

uint32_t joystick_report_interval_us(joystick_speed_t speed, bool autofire_active) {
    return speed == JOY_SPEED_SLOW && !autofire_active
        ? JOY_SLOW_REPORT_INTERVAL_US : JOY_FAST_REPORT_INTERVAL_US;
}

joystick_report_t joystick_make_report(autofire_state_t *state, uint8_t inputs,
                                       const joystick_profile_t *profile,
                                       bool suppress_fire) {
    bool up = false, down = false, left = false, right = false;
    uint8_t buttons = 0;
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        const ini_binding_t *binding = input_binding(profile, input);
        if (suppress_fire || !binding_effective(state, profile, input, inputs)) continue;
        if (binding->type == INI_BIND_AXIS) {
            if (binding->value == INPUT_UP) up = true;
            else if (binding->value == INPUT_DOWN) down = true;
            else if (binding->value == INPUT_LEFT) left = true;
            else if (binding->value == INPUT_RIGHT) right = true;
        } else if (binding->type == INI_BIND_GAMEPAD && binding->value <= 4) {
            buttons |= (uint8_t)(1u << (binding->value - 1));
        }
    }
    return (joystick_report_t){
        .x = (left == right) ? 0 : (left ? -127 : 127),
        .y = (up == down) ? 0 : (up ? -127 : 127),
        .buttons = buttons
    };
}

joystick_keyboard_report_t joystick_make_keyboard_report(
    autofire_state_t *state, uint8_t inputs, const joystick_profile_t *profile,
    bool suppress_fire) {
    joystick_keyboard_report_t report = {0};
    if (suppress_fire) return report;
    unsigned count = 0;
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        const ini_binding_t *binding = input_binding(profile, input);
        uint8_t mask = (uint8_t)(1u << input);
        bool keyboard_tap = !binding->autofire && (state->keyboard_tap_mask & mask);
        bool effective = binding->autofire
            ? binding_effective(state, profile, input, inputs) : keyboard_tap;
        if (!effective || binding->type != INI_BIND_KEYBOARD) continue;
        report.modifier |= binding->modifier;
        if (binding->value == INI_CODE_NONE) continue;
        bool duplicate = false;
        for (unsigned k = 0; k < count; ++k) {
            if (report.keycodes[k] == ini_config_keycode(binding->value)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && count < 6) report.keycodes[count++] = ini_config_keycode(binding->value);
    }
    return report;
}

joystick_runtime_output_t joystick_runtime_step(
    autofire_state_t *state, uint8_t inputs, const joystick_profile_t *profile,
    const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT], joystick_speed_t speed,
    bool direct_active, bool suppress_fire, uint32_t now_us) {
    autofire_state_update(state, inputs, profile, fixed_rates, now_us);
    return (joystick_runtime_output_t){
        .joystick = joystick_make_report(state, inputs, profile, suppress_fire),
        .keyboard = joystick_make_keyboard_report(state, inputs, profile, suppress_fire),
        .report_interval_us = joystick_report_interval_us(speed, state->active),
        .autofire_held = state->active,
        .led_active = joystick_status_led_active(direct_active, state->active,
                                                  state->pulse)
    };
}
