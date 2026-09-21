#include "joystick.h"

#include <stddef.h>
#include <string.h>

#include "config.h"
#include "ini_config.h"
#include "settings.h"

_Static_assert(sizeof(joystick_report_t) == 3, "HID report must be 3 bytes");
_Static_assert(offsetof(joystick_report_t, x) == 0 &&
               offsetof(joystick_report_t, y) == 1 &&
               offsetof(joystick_report_t, buttons) == 2,
               "HID report fields must match descriptor order");
_Static_assert(sizeof(joystick_keyboard_report_t) == 8,
               "HID keyboard report must be 8 bytes");
_Static_assert(JOY_DEBOUNCE_MS > 0 && JOY_FAST_REPORT_INTERVAL_US > 0 &&
               JOY_SLOW_REPORT_INTERVAL_US > 0,
               "Input and report intervals must be positive");
_Static_assert(JOY_AUTOFIRE_MIN_HZ > 0 && JOY_AUTOFIRE_MIN_HZ <= JOY_AUTOFIRE_MAX_HZ &&
               JOY_AUTOFIRE_MAX_HZ <= 500,
               "Autofire frequency range must be between 1 and 500 Hz");
_Static_assert(JOY_AUTOFIRE_DEFAULT_HZ >= JOY_AUTOFIRE_MIN_HZ &&
               JOY_AUTOFIRE_DEFAULT_HZ <= JOY_AUTOFIRE_MAX_HZ,
               "Autofire default must be inside the configured frequency range");
_Static_assert(JOY_AUTOFIRE_REPEAT_MS > 0,
               "Autofire repeat interval must be positive");
_Static_assert(JOY_GESTURE_ACTIVATION_DELAY_MS > 0,
               "Gesture activation delay must be positive");
_Static_assert(JOY_SPECIAL_HOLD_MS > 0 && JOY_CONFIG_MODE_HOLD_MS > 0,
               "Gesture hold intervals must be positive");
#define INPUT_GPIO_MASK ((1u << JOY_GPIO_UP) | (1u << JOY_GPIO_DOWN) | \
                        (1u << JOY_GPIO_LEFT) | (1u << JOY_GPIO_RIGHT) | \
                        (1u << JOY_GPIO_BIG_FIRE_1) | (1u << JOY_GPIO_BIG_FIRE_2) | \
                        (1u << JOY_GPIO_SMALL_FIRE_1) | (1u << JOY_GPIO_SMALL_FIRE_2))
_Static_assert(JOY_GPIO_UP < 30 && JOY_GPIO_DOWN < 30 && JOY_GPIO_LEFT < 30 &&
               JOY_GPIO_RIGHT < 30 && JOY_GPIO_BIG_FIRE_1 < 30 &&
               JOY_GPIO_BIG_FIRE_2 < 30 && JOY_GPIO_SMALL_FIRE_1 < 30 &&
               JOY_GPIO_SMALL_FIRE_2 < 30 && JOY_GPIO_STATUS_LED < 30,
               "GPIO numbers must be valid RP2040 pins");
_Static_assert((1u << JOY_GPIO_UP) + (1u << JOY_GPIO_DOWN) +
               (1u << JOY_GPIO_LEFT) + (1u << JOY_GPIO_RIGHT) +
               (1u << JOY_GPIO_BIG_FIRE_1) + (1u << JOY_GPIO_BIG_FIRE_2) +
               (1u << JOY_GPIO_SMALL_FIRE_1) + (1u << JOY_GPIO_SMALL_FIRE_2) ==
               INPUT_GPIO_MASK, "Input GPIOs must be distinct");
_Static_assert((INPUT_GPIO_MASK & (1u << JOY_GPIO_STATUS_LED)) == 0,
               "LED GPIO cannot be an input GPIO");
_Static_assert((INPUT_GPIO_MASK & (1u << JOY_UPDATE_GPIO_A)) &&
               (INPUT_GPIO_MASK & (1u << JOY_UPDATE_GPIO_B)) &&
               JOY_UPDATE_GPIO_A != JOY_UPDATE_GPIO_B,
               "Update GPIOs must select distinct configured inputs");

static const uint8_t input_gpios[INPUT_COUNT] = {
    JOY_GPIO_UP, JOY_GPIO_DOWN, JOY_GPIO_LEFT, JOY_GPIO_RIGHT,
    JOY_GPIO_BIG_FIRE_1, JOY_GPIO_BIG_FIRE_2,
    JOY_GPIO_SMALL_FIRE_1, JOY_GPIO_SMALL_FIRE_2
};

/* Logical fire button N (1..4, the "big fire" pair then the "small fire"
 * pair) maps to physical inputs INI-order (button_code index 0..3). */
static const input_id_t fire_inputs[4] = {
    INPUT_BIG_FIRE_1, INPUT_BIG_FIRE_2,
    INPUT_SMALL_FIRE_1, INPUT_SMALL_FIRE_2
};

bool joystick_input_pressed(uint8_t inputs, input_id_t input) {
    return (inputs & (1u << input)) != 0;
}

uint8_t joystick_input_gpio(input_id_t input) {
    return input_gpios[input];
}

input_id_t joystick_fire_input(unsigned fire_button) {
    return fire_button < 4 ? fire_inputs[fire_button] : INPUT_COUNT;
}

uint8_t joystick_gpio_snapshot(uint32_t gpio_levels) {
    uint8_t pressed = 0;
    for (unsigned i = 0; i < INPUT_COUNT; ++i) {
        if ((gpio_levels & (1u << input_gpios[i])) == 0) pressed |= 1u << i;
    }
    return pressed;
}

void input_filter_init(input_filter_t *filter, uint8_t raw) {
    filter->stable = raw;
    filter->candidate = raw;
    for (unsigned i = 0; i < INPUT_COUNT; ++i) filter->changed_at_us[i] = 0;
}

uint8_t input_filter_update(input_filter_t *filter, uint8_t raw, uint32_t now_us) {
    for (unsigned i = 0; i < INPUT_COUNT; ++i) {
        uint8_t mask = 1u << i;
        if ((raw & mask) != (filter->candidate & mask)) {
            filter->candidate ^= mask;
            filter->changed_at_us[i] = now_us;
        } else if ((raw & mask) != (filter->stable & mask) &&
                   (uint32_t)(now_us - filter->changed_at_us[i]) >=
                       JOY_DEBOUNCE_MS * 1000u) {
            filter->stable ^= mask;
        }
    }
    return filter->stable;
}

bool joystick_gpio_pressed(uint8_t inputs, uint8_t gpio) {
    for (unsigned i = 0; i < INPUT_COUNT; ++i) {
        if (input_gpios[i] == gpio) return joystick_input_pressed(inputs, i);
    }
    return false;
}

void autofire_state_init(autofire_state_t *state) {
    *state = (autofire_state_t){ .hz = JOY_AUTOFIRE_DEFAULT_HZ };
}

void autofire_state_update(autofire_state_t *state, bool enabled, uint32_t now_us) {
    uint32_t hz = state->hz ? state->hz : JOY_AUTOFIRE_DEFAULT_HZ;
    const uint32_t half_period_us = 500000u / hz;
    if (!enabled) {
        state->active = false;
        state->pulse = false;
    } else if (!state->active) {
        state->active = true;
        state->pulse = true;
        state->last_toggle_us = now_us;
    } else {
        uint32_t periods = (uint32_t)(now_us - state->last_toggle_us) / half_period_us;
        if (periods) {
            if (periods & 1u) state->pulse = !state->pulse;
            state->last_toggle_us += periods * half_period_us;
        }
    }
}

static const ini_binding_t *input_binding(const joystick_profile_t *profile,
                                          unsigned input) {
    return input < JOY_DIRECTION_COUNT
        ? &profile->direction[input] : &profile->button[input - JOY_DIRECTION_COUNT];
    return &profile->button[0];
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

/* A button that is autofiring outputs only while the shared pulse is high.
 * Autofire wins over a normal button mapped to the same output: while an
 * autofire button is held, any normal button with the same code contributes
 * nothing, so the shared output follows only the autofire pattern. Two buttons
 * emit the same output exactly when their codes are equal (the autofire flag
 * lives separately in the mask). */
static bool binding_equal(const ini_binding_t *a, const ini_binding_t *b) {
    return a->type == b->type && a->value == b->value && a->modifier == b->modifier;
}

static bool binding_effective(const autofire_state_t *state,
                              const joystick_profile_t *profile,
                              unsigned input, uint8_t inputs) {
    const ini_binding_t *binding = input_binding(profile, input);
    if (!joystick_input_pressed(inputs, (input_id_t)input)) return false;
    if (binding->autofire) return state->pulse;
    for (unsigned other = 0; other < INPUT_COUNT; ++other) {
        const ini_binding_t *candidate = input_binding(profile, other);
        if (other != input && candidate->autofire &&
            joystick_input_pressed(inputs, (input_id_t)other) &&
            binding_equal(candidate, binding)) {
            return false;
        }
    }
    return true;
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
    joystick_report_t report = {
        .x = (left == right) ? 0 : (left ? -127 : 127),
        .y = (up == down) ? 0 : (up ? -127 : 127),
        .buttons = buttons
    };
    return report;
}

joystick_keyboard_report_t joystick_make_keyboard_report(autofire_state_t *state,
                                       uint8_t inputs,
                                       const joystick_profile_t *profile,
                                       bool suppress_fire) {
    joystick_keyboard_report_t report = {0};
    if (suppress_fire) return report;
    unsigned count = 0;
    for (unsigned input = 0; input < INPUT_COUNT; ++input) {
        const ini_binding_t *binding = input_binding(profile, input);
        if (!binding_effective(state, profile, input, inputs) ||
            binding->type != INI_BIND_KEYBOARD) continue;
        report.modifier |= binding->modifier;
        if (binding->value == INI_CODE_NONE) continue;
        bool duplicate = false;
        for (unsigned k = 0; k < count; ++k) {
            if (report.keycodes[k] == ini_config_keycode(binding->value)) { duplicate = true; break; }
        }
        if (!duplicate && count < 6) report.keycodes[count++] = ini_config_keycode(binding->value);
    }
    return report;
}

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
    bool profile_pair = s1 && s2 && profile_direction &&
                        !b1 && !b2;
    uint8_t selected_profile = 0;
    if (profile_pair) {
        if (joystick_input_pressed(inputs, INPUT_UP)) selected_profile = 0;
        else if (joystick_input_pressed(inputs, INPUT_DOWN)) selected_profile = 1;
        else if (joystick_input_pressed(inputs, INPUT_LEFT)) selected_profile = 2;
        else selected_profile = 3;
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
    } else {
        bool direction_changed = state->rate_adjust_held &&
                                 state->rate_decrease != decrease;
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
    /* Big Fire 1 + Big Fire 2 for 3s toggles LED feedback. */
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

/* Select-on-release gesture on the two update (small fire) buttons. While both
 * are held the live mode is config once JOY_SPECIAL_HOLD_MS elapses and
 * firmware once JOY_CONFIG_MODE_HOLD_MS more elapses. The chosen mode is
 * returned exactly once when the buttons are released; releasing too early
 * returns BOOT_MODE_NONE. */
boot_mode_action_t boot_mode_step(boot_mode_state_t *state, bool both_pressed,
                                  uint32_t now_us) {
    boot_mode_action_t action = BOOT_MODE_NONE;
    if (both_pressed) {
        state->fired = false;
        if (!state->held) {
            state->held = true;
            state->hold_start_us = now_us;
        }
        uint32_t elapsed = now_us - state->hold_start_us;
        if (elapsed >= (JOY_SPECIAL_HOLD_MS + JOY_CONFIG_MODE_HOLD_MS) * 1000u) {
            state->mode = BOOT_MODE_FIRMWARE;
        } else if (elapsed >= JOY_SPECIAL_HOLD_MS * 1000u) {
            state->mode = BOOT_MODE_CONFIG;
        } else {
            state->mode = BOOT_MODE_NONE;
        }
    } else {
        if (state->held && !state->fired) {
            action = (boot_mode_action_t)state->mode;
            state->fired = true;
        }
        state->held = false;
        state->mode = BOOT_MODE_NONE;
    }
    return action;
}

bool config_mode_exit_step(config_exit_state_t *state, bool both_pressed,
                           uint32_t now_us) {
    if (!both_pressed) {
        state->held = false;
        state->triggered = false;
        return false;
    }
    if (!state->held) {
        state->held = true;
        state->triggered = false;
        state->hold_start_us = now_us;
    }
    if (!state->triggered &&
        (uint32_t)(now_us - state->hold_start_us) >=
            JOY_SPECIAL_HOLD_MS * 1000u) {
        state->triggered = true;
        return true;
    }
    return false;
}

bool factory_reset_step(factory_reset_state_t *state, bool all_pressed,
                        uint32_t now_us) {
    if (!all_pressed) {
        state->held = false;
        state->triggered = false;
        return false;
    }
    if (!state->held) {
        state->held = true;
        state->triggered = false;
        state->hold_start_us = now_us;
    }
    if (!state->triggered &&
        (uint32_t)(now_us - state->hold_start_us) >= JOY_SPECIAL_HOLD_MS * 1000u) {
        state->triggered = true;
        return true;
    }
    return false;
}

bool joystick_report_due(uint32_t *next_us, uint32_t now_us, uint32_t interval_us) {
    if ((int32_t)(now_us - *next_us) < 0) return false;
    *next_us += ((uint32_t)(now_us - *next_us) / interval_us + 1u) * interval_us;
    return true;
}
