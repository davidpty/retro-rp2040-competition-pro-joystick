int test_ini_config_main(void);
int test_joystick_main(void);
int test_msc_volume_main(void);
int test_led_color_main(void);

int main(void) {
    int rc = test_ini_config_main();
    rc |= test_joystick_main();
    rc |= test_msc_volume_main();
    rc |= test_led_color_main();
    return rc;
}
