#ifndef JOYSTICK_REPORTS_H_
#define JOYSTICK_REPORTS_H_
#include "joystick_types.h"
uint32_t joystick_report_interval_us(joystick_speed_t speed, bool autofire_active);
joystick_report_t joystick_make_report(autofire_state_t *state, uint8_t inputs,
                                       const joystick_profile_t *profile,
                                       bool suppress_fire);
joystick_keyboard_report_t joystick_make_keyboard_report(
    autofire_state_t *state, uint8_t inputs,
    const joystick_profile_t *profile, bool suppress_fire);
joystick_runtime_output_t joystick_runtime_step(
    autofire_state_t *state, uint8_t inputs, const joystick_profile_t *profile,
    const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT], joystick_speed_t speed,
    bool direct_active, bool suppress_fire, uint32_t now_us);
#endif
