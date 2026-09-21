#ifndef JOYSTICK_AUTOFIRE_H_
#define JOYSTICK_AUTOFIRE_H_
#include "joystick_types.h"
void autofire_state_init(autofire_state_t *state);
void autofire_state_update(autofire_state_t *state, uint8_t inputs,
                           const joystick_profile_t *profile,
                           const uint8_t fixed_rates[JOY_PROFILE_INPUT_COUNT],
                           uint32_t now_us);
bool joystick_autofire_enabled(uint8_t inputs, const joystick_profile_t *profile);
#endif
