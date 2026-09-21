#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ini_config.h"

static const char config_text[] =
    "[RED]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\n"
    "button1=SHIFT+A\nbutton2=JOY1\nbutton3=CTRL+ALT+B:AUTOFIRE:250MS\nbutton4=NONE\n"
    "[GREEN]\nup=W\ndown=S\nleft=A\nright=D\n"
    "button1=JOY2\nbutton2=SPACE\nbutton3=SHIFT\nbutton4=NONE\n"
    "[BLUE]\nup=JOY1\ndown=JOY2\nleft=UP:AUTOFIRE\nright=RIGHT\n"
    "button1=CTRL+F1\nbutton2=NONE\nbutton3=JOY3\nbutton4=Z\n"
    "[YELLOW]\nup=NONE\ndown=NONE\nleft=NONE\nright=NONE\n"
    "button1=A\nbutton2=B\nbutton3=C\nbutton4=D\n";

static void test_parse_bindings(void) {
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    assert(ini_config_parse((const uint8_t *)config_text, strlen(config_text), bindings));
    assert(bindings[0][0].type == INI_BIND_AXIS && bindings[0][0].value == 0);
    assert(bindings[0][4].type == INI_BIND_KEYBOARD &&
           bindings[0][4].modifier == 0x02 && bindings[0][4].value == INI_CODE_A);
    assert(bindings[0][6].modifier == (0x01 | 0x04) &&
           bindings[0][6].value == INI_CODE_A + 1 && bindings[0][6].autofire &&
           bindings[0][6].autofire_delay_ms == 250);
    assert(bindings[1][0].type == INI_BIND_KEYBOARD &&
           ini_config_keycode(bindings[1][0].value) == 0x1a); /* W */
    assert(bindings[2][2].type == INI_BIND_AXIS && bindings[2][2].autofire);
}

static void test_autofire_rates(void) {
    static const char text[] =
        "[RED]\nup=UP:AUTOFIRE:500MS:50HZ\ndown=DOWN:AUTOFIRE:100HZ:250MS\n"
        "left=LEFT\nright=RIGHT\nbutton1=JOY1\nbutton2=JOY2\nbutton3=JOY3\nbutton4=JOY4\n"
        "[GREEN]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\nbutton1=JOY1\nbutton2=JOY2\nbutton3=JOY3\nbutton4=JOY4\n"
        "[BLUE]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\nbutton1=JOY1\nbutton2=JOY2\nbutton3=JOY3\nbutton4=JOY4\n"
        "[YELLOW]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\nbutton1=JOY1\nbutton2=JOY2\nbutton3=JOY3\nbutton4=JOY4\n";
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    uint8_t rates[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT] = {{0}};
    assert(ini_config_parse_with_rates((const uint8_t *)text, strlen(text),
                                       bindings, rates));
    assert(bindings[0][0].autofire && bindings[0][0].autofire_delay_ms == 500);
    assert(rates[0][0] == 50);
    assert(bindings[0][1].autofire && bindings[0][1].autofire_delay_ms == 250);
    assert(rates[0][1] == 100);
}

static void test_format_bindings(void) {
    char text[64];
    ini_binding_t binding = {INI_BIND_KEYBOARD, INI_CODE_A, 0x02, 1, 0};
    assert(ini_config_binding_format(&binding, text, sizeof(text)));
    assert(strcmp(text, "SHIFT+A:AUTOFIRE") == 0);
    binding.autofire_delay_ms = 1250;
    assert(ini_config_binding_format(&binding, text, sizeof(text)));
    assert(strcmp(text, "SHIFT+A:AUTOFIRE:1250MS") == 0);
    assert(ini_config_binding_format_with_rate(&binding, 20, text, sizeof(text)));
    assert(strcmp(text, "SHIFT+A:AUTOFIRE:1250MS:20HZ") == 0);
    binding = (ini_binding_t){INI_BIND_AXIS, 1, 0, 0, 0};
    assert(ini_config_binding_format(&binding, text, sizeof(text)));
    assert(strcmp(text, "DOWN") == 0);
    assert(!ini_config_binding_format(&binding, text, 0));
    assert(!ini_config_binding_format(&binding, text, 1));

    binding = (ini_binding_t){INI_BIND_KEYBOARD, INI_CODE_A, 0x07, 1, 60000};
    assert(ini_config_binding_format_with_rate(&binding, 100, text, sizeof(text)));
    assert(strcmp(text, "CTRL+SHIFT+ALT+A:AUTOFIRE:60000MS:100HZ") == 0);
}

static void test_invalid_configs(void) {
    ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT];
    const char *invalid = "[RED]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\n"
                          "button1=SHIFT+A+B\nbutton2=JOY1\nbutton3=JOY2\nbutton4=JOY3\n";
    assert(!ini_config_parse((const uint8_t *)invalid, strlen(invalid), bindings));
    invalid = "[RED]\nup=UP:AUTOFIRE\ndown=DOWN\nleft=LEFT\nright=RIGHT\n"
              "button1=A\nbutton2=B\nbutton3=C\nbutton4=D\n";
    assert(!ini_config_parse((const uint8_t *)invalid, strlen(invalid), bindings));
    invalid = "[RED]\nup=UP\ndown=DOWN\nleft=LEFT\nright=RIGHT\n"
              "button1=A:AUTOFIRE:0MS\nbutton2=B\nbutton3=C\nbutton4=D\n";
    assert(!ini_config_parse((const uint8_t *)invalid, strlen(invalid), bindings));
}

int test_ini_config_main(void) {
    test_parse_bindings();
    test_autofire_rates();
    test_format_bindings();
    test_invalid_configs();
    puts("ini config tests passed");
    return 0;
}
