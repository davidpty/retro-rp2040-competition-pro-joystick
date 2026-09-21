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
        (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 0, 0};
    settings.profiles[0].direction[0] =
        (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A + 22, 0, 0, 0};
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

static void test_volume_boundary_reads(void) {
    msc_volume_t volume;
    joystick_settings_t settings;
    uint8_t data[MSC_DISK_BLOCK_SIZE];
    joystick_settings_defaults(&settings);
    msc_volume_rebuild(&volume, &settings);
    assert(msc_volume_read_ini(&volume, data, 0) == 0);

    /* A corrupt start cluster must be rejected without reading outside the
     * fixed disk image. */
    volume.disk[2][32 + 26] = 0xff;
    volume.disk[2][32 + 27] = 0xff;
    assert(msc_volume_read_ini(&volume, data, sizeof(data)) == 0);
}

int test_msc_volume_main(void) {
    test_volume_round_trip();
    test_volume_boundary_reads();
    puts("msc volume tests passed");
    return 0;
}
