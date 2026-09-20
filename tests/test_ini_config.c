#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ini_config.h"

static bool parse_ok(const char *text,
                     uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT],
                     uint8_t masks[JOY_PROFILE_COUNT]) {
    return ini_config_parse((const uint8_t *)text, strlen(text), codes, masks);
}

static void test_round_trip_names(void) {
    assert(strcmp(ini_config_code_name(INI_CODE_NONE), "NONE") == 0);
    assert(strcmp(ini_config_code_name(INI_CODE_JOY4), "JOY4") == 0);
    assert(strcmp(ini_config_code_name(INI_CODE_A + 25), "Z") == 0);
    assert(strcmp(ini_config_code_name(INI_CODE_F1 + 11), "F12") == 0);
}

static void test_valid_config(void) {
    const char *text =
        "; profiles\r\n"
        "[RED]\r\nbutton1=A\r\nbutton2=JOY2\r\nbutton3=F5\r\nbutton4=NONE\r\n"
        "[GREEN]\r\nbutton1=SPACE:AUTOFIRE\r\nbutton2=JOY2\r\nbutton3=JOY3\r\nbutton4=JOY4\r\n"
        "[PURPLE]\r\nbutton1=JOY1\r\nbutton2=SHIFT:AUTOFIRE\r\nbutton3=BACKSPACE\r\nbutton4=0\r\n"
        "[YELLOW]\r\nbutton1=a:autofire\r\nbutton2=joy1\r\nbutton3=f12\r\nbutton4=space\r\n";
    uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT];
    uint8_t masks[JOY_PROFILE_COUNT];
    assert(parse_ok(text, codes, masks));
    assert(codes[0][0] == INI_CODE_A && codes[0][2] == INI_CODE_F1 + 4);
    assert(codes[1][0] == INI_CODE_SPACE && masks[1] == 1);
    assert(codes[2][1] == INI_CODE_SHIFT && masks[2] == 2);
    assert(codes[3][0] == INI_CODE_A && masks[3] == 1);
}

static void test_invalid_configs(void) {
    uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT];
    uint8_t masks[JOY_PROFILE_COUNT];
    const char *base =
        "[RED]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n"
        "[GREEN]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n"
        "[PURPLE]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n"
        "[YELLOW]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n";
    assert(parse_ok(base, codes, masks));
    const char *missing =
        "[RED]\nbutton1=A\nbutton2=B\nbutton3=C\n"
        "[GREEN]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n"
        "[PURPLE]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n"
        "[YELLOW]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n";
    assert(!parse_ok(missing, codes, masks));
    assert(!parse_ok("[RED]\nbutton1=A\n", codes, masks));
    assert(!parse_ok("[BLUE]\nbutton1=A\n", codes, masks));
    assert(!parse_ok("[RED]\nbutton1=A:BAD\nbutton2=B\nbutton3=C\nbutton4=D\n",
                     codes, masks));
    assert(!parse_ok("[CONFIG]\nbutton1=A\nbutton2=B\nbutton3=C\nbutton4=D\n",
                     codes, masks));
}

static void test_keycode_mapping(void) {
    assert(ini_config_joy_button(INI_CODE_JOY1) == 1);
    assert(ini_config_keycode(INI_CODE_A) == 0x04);
    assert(ini_config_keycode(INI_CODE_F1 + 11) == 0x45);
    assert(ini_config_modifier(INI_CODE_SHIFT) == 0x02);
}

int test_ini_config_main(void) {
    test_round_trip_names();
    test_valid_config();
    test_invalid_configs();
    test_keycode_mapping();
    puts("ini config tests passed");
    return 0;
}
