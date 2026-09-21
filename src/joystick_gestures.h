#ifndef JOYSTICK_GESTURES_H_
#define JOYSTICK_GESTURES_H_
#include "joystick_types.h"
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
#endif
