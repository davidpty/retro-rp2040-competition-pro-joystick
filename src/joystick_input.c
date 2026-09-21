#include "joystick_input.h"

#include "config.h"

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
