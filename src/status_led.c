#include "status_led.h"

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "config.h"
#include "led_color.h"
#include "ws2812.pio.h"

static PIO led_pio;
static uint led_sm;
static uint32_t led_color;
static uint32_t led_last_tx_us;
static uint32_t led_hold_color;
static joy_profile_id_t led_profile;
static bool led_config_mode;

/* Profile palette; index selects Red, Green, Blue, or Yellow. */
static const uint32_t profile_colors[JOY_PROFILE_COUNT] = {
    [JOY_PROFILE_RED] = JOY_PROFILE_COLOR_0,
    [JOY_PROFILE_GREEN] = JOY_PROFILE_COLOR_1,
    [JOY_PROFILE_BLUE] = JOY_PROFILE_COLOR_2,
    [JOY_PROFILE_YELLOW] = JOY_PROFILE_COLOR_3
};

static uint32_t profile_color(void) {
    return profile_colors[led_profile];
}

/* A 24-bit WS2812 frame takes 30 us; allow more than 50 us low to latch. */
#define LED_FRAME_GAP_US 100u
#define LED_REFRESH_US 20000u
#define LED_REJECTION_FLASH_MS 150u
#define LED_REJECTION_PAUSE_MS 400u

void status_led_init(void) {
    led_pio = pio0;
    led_sm = pio_claim_unused_sm(led_pio, true);
    uint offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, offset, JOY_GPIO_STATUS_LED, 800000.0f);
    pio_sm_put(led_pio, led_sm, 0);
    led_last_tx_us = time_us_32();
}

static void status_led_send_blocking(uint32_t color) {
    while (!pio_sm_is_tx_fifo_empty(led_pio, led_sm)) tight_loop_contents();
    pio_sm_put(led_pio, led_sm, color);
    led_color = color;
    led_last_tx_us = time_us_32();
}

void status_led_set_profile(uint8_t index) {
    led_profile = index < JOY_PROFILE_COUNT ? (joy_profile_id_t)index : JOY_PROFILE_RED;
}

void status_led_startup_blink(bool led_enabled, bool slow_mode) {
    if (led_config_mode) {
        status_led_send_blocking(JOY_LED_CONFIG_COLOR);
        return;
    }
    if (!led_enabled) return;

    /* One profile-colored flash confirms startup and shows the saved polling
     * mode: full profile color for fast mode, dimmed for slow mode. */
    uint32_t profile = profile_color();
    uint32_t mode_color = slow_mode ? led_color_scale(profile, JOY_PROFILE_SLOW_BRIGHTNESS)
                                    : profile;
    status_led_send_blocking(JOY_LED_IDLE_COLOR);
    sleep_ms(250);
    status_led_send_blocking(mode_color);
    sleep_ms(500);
    status_led_send_blocking(JOY_LED_IDLE_COLOR);
}

void status_led_set_config_mode(bool enabled) {
    led_config_mode = enabled;
    if (enabled) status_led_send_blocking(JOY_LED_CONFIG_COLOR);
}

void status_led_rejection_blink(void) {
    for (unsigned i = 0; i < 3; ++i) {
        status_led_send_blocking(JOY_LED_WARNING_COLOR);
        sleep_ms(LED_REJECTION_FLASH_MS);
        status_led_send_blocking(JOY_LED_IDLE_COLOR);
        sleep_ms(LED_REJECTION_FLASH_MS);
    }
    sleep_ms(LED_REJECTION_PAUSE_MS);
    if (led_config_mode) status_led_send_blocking(JOY_LED_CONFIG_COLOR);
}

void status_led_set_hold_color(uint32_t color) {
    /* Clear with 0 to return to automatic colors. Nonzero takes precedence
     * (e.g. the config/firmware mode-selection confirmation). */
    led_hold_color = color;
}

void status_led_update(bool active, bool led_enabled, bool slow_mode,
                       uint32_t now_us) {
    uint32_t color;
    if (led_config_mode) {
        color = JOY_LED_CONFIG_COLOR;
    } else if (led_hold_color != 0) {
        color = led_hold_color;
    } else {
        color = !led_enabled ? JOY_LED_IDLE_COLOR :
                (active ? (slow_mode
                               ? led_color_scale(profile_color(), JOY_PROFILE_SLOW_BRIGHTNESS)
                               : profile_color())
                        : JOY_LED_IDLE_COLOR);
    }
    uint32_t elapsed_us = now_us - led_last_tx_us;
    if (elapsed_us < LED_FRAME_GAP_US) return;
    if (color == led_color && elapsed_us < LED_REFRESH_US) return;
    if (pio_sm_is_tx_fifo_empty(led_pio, led_sm)) {
        pio_sm_put(led_pio, led_sm, color);
        led_color = color;
        led_last_tx_us = now_us;
    }
}
