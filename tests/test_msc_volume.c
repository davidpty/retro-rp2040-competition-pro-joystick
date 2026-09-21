#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "msc_volume.h"
#include "settings.h"

static void test_volume_round_trip(void) {
    msc_volume_t volume;
    joystick_settings_t settings;
    joystick_settings_defaults(&settings);
    settings.profiles[0].button[0] =
        (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 0};
    settings.profiles[0].direction[0] =
        (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A + 22, 0, 0};
    msc_volume_rebuild(&volume, &settings);
    assert(volume.ini_size > MSC_DISK_BLOCK_SIZE);
    uint8_t data[MSC_DISK_BLOCK_SIZE * 2];
    size_t length = msc_volume_read_ini(&volume, data, sizeof(data));
    assert(length == volume.ini_size);
    assert(length < sizeof(data));
    data[length] = '\0';
    char *red = strstr((char *)data, "[RED]\r\n");
    assert(red != NULL);
    assert(strstr(red, "button1=") < strstr(red, "up="));
    assert(strstr(red, "button4=") < strstr(red, "up="));
    assert(strstr(red, "button3=JOY1:AUTOFIRE\r\n") != NULL);
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    assert(ini_config_parse(data, length, bindings));
    assert(bindings[0][0].type == INI_BIND_KEYBOARD && bindings[0][0].value == INI_CODE_A + 22);
    assert(bindings[0][4].modifier == 0x02 && bindings[0][4].value == INI_CODE_A);
}

int test_msc_volume_main(void) {
    test_volume_round_trip();
    puts("msc volume tests passed");
    return 0;
}
