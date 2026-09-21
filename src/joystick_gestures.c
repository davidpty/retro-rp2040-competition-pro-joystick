#include "joystick_gestures.h"

#include "config.h"

/* The short hold-state machines are kept separate from report generation so
 * timing behavior can be tested without pulling in the HID implementation. */

/* The two update (small fire) buttons select config mode on release after
 * JOY_SPECIAL_HOLD_MS, but enter firmware update mode immediately when the
 * full six-second hold is reached. Releasing too early returns NONE. */
boot_mode_action_t boot_mode_step(boot_mode_state_t *state, bool both_pressed,
                                  uint32_t now_us) {
    boot_mode_action_t action = BOOT_MODE_NONE;
    if (both_pressed) {
        if (!state->held) {
            state->held = true;
            state->fired = false;
            state->hold_start_us = now_us;
        }
        uint32_t elapsed = now_us - state->hold_start_us;
        if (elapsed >= (JOY_SPECIAL_HOLD_MS + JOY_CONFIG_MODE_HOLD_MS) * 1000u) {
            state->mode = BOOT_MODE_FIRMWARE;
            if (!state->fired) {
                state->fired = true;
                action = BOOT_MODE_FIRMWARE;
            }
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
