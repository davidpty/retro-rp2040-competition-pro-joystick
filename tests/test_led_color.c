#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

#include "config.h"
#include "led_color.h"

static void test_slow_profile_colors(void) {
    uint8_t level = JOY_PROFILE_SLOW_BRIGHTNESS;
    assert(led_color_scale(JOY_PROFILE_COLOR_0, level) ==
           JOY_LED_RGB(51, 0, 0));
    assert(led_color_scale(JOY_PROFILE_COLOR_1, level) ==
           JOY_LED_RGB(0, 51, 0));
    assert(led_color_scale(JOY_PROFILE_COLOR_2, level) ==
           JOY_LED_RGB(0, 0, 51));
    assert(led_color_scale(JOY_PROFILE_COLOR_3, level) ==
           JOY_LED_RGB(51, 40, 0));
}

int test_led_color_main(void) {
    assert(led_color_scale(JOY_PROFILE_COLOR_1, 255) == JOY_PROFILE_COLOR_1);
    test_slow_profile_colors();
    puts("led color tests passed");
    return 0;
}
