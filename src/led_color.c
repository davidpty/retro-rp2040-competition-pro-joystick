#include "led_color.h"

#include "config.h"

uint32_t led_color_scale(uint32_t color, uint8_t level) {
    uint8_t red = (uint8_t)(color >> 16);
    uint8_t green = (uint8_t)(color >> 24);
    uint8_t blue = (uint8_t)(color >> 8);
    red = (uint8_t)(((uint32_t)red * level + 127u) / 255u);
    green = (uint8_t)(((uint32_t)green * level + 127u) / 255u);
    blue = (uint8_t)(((uint32_t)blue * level + 127u) / 255u);
    return JOY_LED_RGB(red, green, blue);
}
