#include "status_led.h"

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "config.h"
#include "ws2812.pio.h"

static PIO led_pio;
static uint led_sm;
static uint32_t led_color;
static uint32_t led_last_tx_us;

/* A 24-bit WS2812 frame takes 30 us; allow more than 50 us low to latch. */
#define LED_FRAME_GAP_US 100u
#define LED_REFRESH_US 20000u

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

void status_led_startup_blink(bool led_enabled, bool slow_mode) {
    if (!led_enabled) return;

    /* One mode-colored flash confirms startup and shows the saved polling
     * mode: reduced red for fast mode or pure blue for slow mode. */
    uint32_t mode_color = slow_mode ? JOY_LED_SLOW_COLOR : JOY_LED_ACTIVE_COLOR;
    status_led_send_blocking(JOY_LED_IDLE_COLOR);
    sleep_ms(250);
    status_led_send_blocking(mode_color);
    sleep_ms(500);
    status_led_send_blocking(JOY_LED_IDLE_COLOR);
}

void status_led_update(bool direct_active, bool autofire_active, bool upload_active,
                       bool led_enabled, bool slow_mode, uint32_t now_us) {
    bool active = direct_active || autofire_active;
    uint32_t color = upload_active ? JOY_LED_UPDATE_COLOR :
                     (!led_enabled ? JOY_LED_IDLE_COLOR :
                     (active ? (slow_mode ? JOY_LED_SLOW_COLOR : JOY_LED_ACTIVE_COLOR)
                             : JOY_LED_IDLE_COLOR));
    uint32_t elapsed_us = now_us - led_last_tx_us;
    if (elapsed_us < LED_FRAME_GAP_US) return;
    if (color == led_color && elapsed_us < LED_REFRESH_US) return;
    if (pio_sm_is_tx_fifo_empty(led_pio, led_sm)) {
        pio_sm_put(led_pio, led_sm, color);
        led_color = color;
        led_last_tx_us = now_us;
    }
}
