#ifndef JOYSTICK_INPUT_H_
#define JOYSTICK_INPUT_H_
#include "joystick_types.h"
uint8_t joystick_gpio_snapshot(uint32_t gpio_levels);
void input_filter_init(input_filter_t *filter, uint8_t raw);
uint8_t input_filter_update(input_filter_t *filter, uint8_t raw, uint32_t now_us);
bool joystick_input_pressed(uint8_t inputs, input_id_t input);
uint8_t joystick_input_gpio(input_id_t input);
bool joystick_gpio_pressed(uint8_t inputs, uint8_t gpio);
input_id_t joystick_fire_input(unsigned fire_button);
#endif
