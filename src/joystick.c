#include "joystick_gestures.h"

#include "config.h"
#include "joystick_input.h"
#include "settings.h"

_Static_assert(JOY_GESTURE_ACTIVATION_DELAY_MS > 0,
               "Gesture activation delay must be positive");
_Static_assert(JOY_SPECIAL_HOLD_MS > 0 && JOY_CONFIG_MODE_HOLD_MS > 0,
               "Gesture hold intervals must be positive");

bool joystick_direct_activity(uint8_t inputs, bool ignore_fire_buttons) {
    bool up = joystick_input_pressed(inputs, INPUT_UP);
    bool down = joystick_input_pressed(inputs, INPUT_DOWN);
    bool left = joystick_input_pressed(inputs, INPUT_LEFT);
    bool right = joystick_input_pressed(inputs, INPUT_RIGHT);
    if (up != down || left != right) return true;
    if (ignore_fire_buttons) return false;
    for (unsigned b = 0; b < 4; ++b) {
        if (joystick_input_pressed(inputs, joystick_fire_input(b))) return true;
    }
    return false;
}

bool joystick_status_led_active(bool direct_active, bool autofire_held,
                                bool autofire_pulse) {
    return autofire_held ? autofire_pulse : direct_active;
}

bool joystick_post_reboot_guard_active(bool guard_active, uint8_t inputs) {
    return guard_active && inputs != 0;
}

bool joystick_gesture_step(gesture_state_t *state, uint8_t inputs,
                           uint32_t now_us, joystick_settings_t *settings) {
    bool b1 = joystick_input_pressed(inputs, INPUT_BIG_FIRE_1);
    bool b2 = joystick_input_pressed(inputs, INPUT_BIG_FIRE_2);
    bool s1 = joystick_input_pressed(inputs, INPUT_SMALL_FIRE_1);
    bool s2 = joystick_input_pressed(inputs, INPUT_SMALL_FIRE_2);
    bool exact_two = (unsigned)b1 + (unsigned)b2 + (unsigned)s1 + (unsigned)s2 == 2;
    bool decrease = exact_two && b1 && s1;
    bool increase = exact_two && b2 && s1;
    bool slow = exact_two && b1 && s2;
    bool fast = exact_two && b2 && s2;
    bool led_pair = exact_two && b1 && b2;
    bool changed = false;
    bool profile_direction = (unsigned)joystick_input_pressed(inputs, INPUT_UP) +
                             (unsigned)joystick_input_pressed(inputs, INPUT_DOWN) +
                             (unsigned)joystick_input_pressed(inputs, INPUT_LEFT) +
                             (unsigned)joystick_input_pressed(inputs, INPUT_RIGHT) == 1;
    bool profile_pair = s1 && s2 && profile_direction && !b1 && !b2;
    joy_profile_id_t selected_profile = JOY_PROFILE_RED;
    if (profile_pair) {
        if (joystick_input_pressed(inputs, INPUT_UP)) selected_profile = JOY_PROFILE_RED;
        else if (joystick_input_pressed(inputs, INPUT_DOWN)) selected_profile = JOY_PROFILE_BLUE;
        else if (joystick_input_pressed(inputs, INPUT_LEFT)) selected_profile = JOY_PROFILE_GREEN;
        else selected_profile = JOY_PROFILE_YELLOW;
        if (!state->profile_held || state->profile_index != selected_profile) {
            state->profile_held = true;
            state->profile_triggered = false;
            state->profile_index = selected_profile;
            state->profile_start_us = now_us;
        }
        if (!state->profile_triggered &&
            (uint32_t)(now_us - state->profile_start_us) >=
                JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u) {
            state->profile_triggered = true;
            if (settings->active_profile != selected_profile) {
                joystick_settings_select_profile(settings, selected_profile);
                changed = true;
            }
            state->release_mask = (uint8_t)((1u << INPUT_SMALL_FIRE_1) |
                                            (1u << INPUT_SMALL_FIRE_2));
            if (joystick_input_pressed(inputs, INPUT_UP)) state->release_mask |= 1u << INPUT_UP;
            if (joystick_input_pressed(inputs, INPUT_DOWN)) state->release_mask |= 1u << INPUT_DOWN;
            if (joystick_input_pressed(inputs, INPUT_LEFT)) state->release_mask |= 1u << INPUT_LEFT;
            if (joystick_input_pressed(inputs, INPUT_RIGHT)) state->release_mask |= 1u << INPUT_RIGHT;
        }
    } else {
        state->profile_held = false;
        state->profile_triggered = false;
    }
    if (!decrease && !increase) {
        state->rate_adjust_held = false;
        state->rate_adjust_active = false;
        state->rate_led_pulse = false;
    } else {
        bool direction_changed = state->rate_adjust_held && state->rate_decrease != decrease;
        if (!state->rate_adjust_held || direction_changed) {
            state->rate_adjust_held = true;
            state->rate_decrease = decrease;
            state->rate_adjust_active = false;
            state->rate_start_us = now_us;
        }
        if (!state->rate_adjust_active &&
            (uint32_t)(now_us - state->rate_start_us) >=
                JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u) {
            state->rate_adjust_active = true;
            state->rate_led_pulse = true;
            state->rate_led_last_toggle_us = now_us;
            state->rate_next_us = now_us + JOY_AUTOFIRE_REPEAT_MS * 1000u;
            if (decrease && settings->rate_hz > JOY_AUTOFIRE_MIN_HZ) {
                --settings->rate_hz;
                changed = true;
            } else if (increase && settings->rate_hz < JOY_AUTOFIRE_MAX_HZ) {
                ++settings->rate_hz;
                changed = true;
            }
        } else if (state->rate_adjust_active &&
                   (int32_t)(now_us - state->rate_next_us) >= 0) {
            do {
                if ((decrease && settings->rate_hz == JOY_AUTOFIRE_MIN_HZ) ||
                    (increase && settings->rate_hz == JOY_AUTOFIRE_MAX_HZ)) {
                    state->rate_next_us = now_us + JOY_AUTOFIRE_REPEAT_MS * 1000u;
                    break;
                }
                if (decrease && settings->rate_hz > JOY_AUTOFIRE_MIN_HZ) {
                    --settings->rate_hz;
                    changed = true;
                } else if (increase && settings->rate_hz < JOY_AUTOFIRE_MAX_HZ) {
                    ++settings->rate_hz;
                    changed = true;
                }
                state->rate_next_us += JOY_AUTOFIRE_REPEAT_MS * 1000u;
            } while ((int32_t)(now_us - state->rate_next_us) >= 0);
        }
        if (changed) state->release_mask = 1u << INPUT_SMALL_FIRE_1;
    }
    bool mode_pair = slow || fast;
    if (!mode_pair) {
        state->mode_held = false;
        state->mode_triggered = false;
    } else if (!state->mode_held) {
        state->mode_held = true;
        state->mode_triggered = false;
        state->mode_slow = slow;
        state->mode_start_us = now_us;
    } else if (state->mode_slow != slow) {
        state->mode_slow = slow;
        state->mode_triggered = false;
        state->mode_start_us = now_us;
    }
    if (mode_pair && !state->mode_triggered &&
        (uint32_t)(now_us - state->mode_start_us) >=
            JOY_GESTURE_ACTIVATION_DELAY_MS * 1000u) {
        state->mode_triggered = true;
        joystick_speed_t selected = slow ? JOY_SPEED_SLOW : JOY_SPEED_FAST;
        if (settings->speed != selected) {
            settings->speed = selected;
            state->release_mask = slow
                ? (uint8_t)((1u << INPUT_BIG_FIRE_1) | (1u << INPUT_SMALL_FIRE_2))
                : (uint8_t)((1u << INPUT_BIG_FIRE_2) | (1u << INPUT_SMALL_FIRE_2));
            changed = true;
        }
    }
    if (!led_pair) {
        state->led_held = false;
        state->led_triggered = false;
    } else {
        if (!state->led_held) {
            state->led_held = true;
            state->led_triggered = false;
            state->led_start_us = now_us;
        }
        if (!state->led_triggered &&
            (uint32_t)(now_us - state->led_start_us) >= JOY_SPECIAL_HOLD_MS * 1000u) {
            state->led_triggered = true;
            settings->led_enabled = !settings->led_enabled;
            state->release_mask = (uint8_t)((1u << INPUT_BIG_FIRE_1) |
                                            (1u << INPUT_BIG_FIRE_2));
            changed = true;
        }
    }
    state->suppress_output =
        (state->rate_adjust_held && state->rate_adjust_active) ||
        (state->mode_held && state->mode_triggered) ||
        (state->led_held && state->led_triggered) ||
        (state->profile_held && state->profile_triggered);
    return changed;
}

bool joystick_rate_adjustment_led_step(gesture_state_t *state, uint8_t rate_hz,
                                       uint32_t now_us) {
    if (!state->rate_adjust_active) {
        state->rate_led_pulse = false;
        state->rate_led_last_toggle_us = now_us;
        return false;
    }
    if (!rate_hz) rate_hz = JOY_AUTOFIRE_DEFAULT_HZ;
    uint32_t half_period_us = 500000u / rate_hz;
    uint32_t periods = (uint32_t)(now_us - state->rate_led_last_toggle_us) / half_period_us;
    if (periods & 1u) state->rate_led_pulse = !state->rate_led_pulse;
    if (periods) state->rate_led_last_toggle_us += periods * half_period_us;
    return state->rate_led_pulse;
}
