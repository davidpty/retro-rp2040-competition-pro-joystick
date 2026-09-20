#include "ini_config.h"

#include <string.h>

/* Case-insensitive string helpers (config content is 7-bit ASCII). */

static char ascii_toupper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool ieq_range(const uint8_t *data, size_t offset, const char *literal,
                      size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (ascii_toupper((char)data[offset + i]) != ascii_toupper(literal[i]))
            return false;
    }
    return true;
}

static bool is_space(uint8_t c) { return c == ' ' || c == '\t'; }

/* Look up a name (case-insensitive) and fill *code. Returns false if unknown. */
static bool name_to_code(const uint8_t *name, size_t length, uint8_t *code) {
    if (length == 4 && ieq_range(name, 0, "NONE", 4)) { *code = INI_CODE_NONE; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY1", 4)) { *code = INI_CODE_JOY1; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY2", 4)) { *code = INI_CODE_JOY2; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY3", 4)) { *code = INI_CODE_JOY3; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY4", 4)) { *code = INI_CODE_JOY4; return true; }
    if (length == 1) {
        char c = ascii_toupper((char)name[0]);
        if (c >= 'A' && c <= 'Z') {
            *code = (uint8_t)(INI_CODE_A + (unsigned)(c - 'A')); return true;
        }
        if (c >= '0' && c <= '9') {
            *code = (uint8_t)(INI_CODE_0 + (unsigned)(c - '0')); return true;
        }
    }
    if (length == 5 && ieq_range(name, 0, "ENTER", 5)) { *code = INI_CODE_ENTER; return true; }
    if (length == 3 && ieq_range(name, 0, "ESC", 3)) { *code = INI_CODE_ESC; return true; }
    if (length == 9 && ieq_range(name, 0, "BACKSPACE", 9)) { *code = INI_CODE_BACKSPACE; return true; }
    if (length == 3 && ieq_range(name, 0, "TAB", 3)) { *code = INI_CODE_TAB; return true; }
    if (length == 5 && ieq_range(name, 0, "SPACE", 5)) { *code = INI_CODE_SPACE; return true; }
    if (length == 5 && ieq_range(name, 0, "SHIFT", 5)) { *code = INI_CODE_SHIFT; return true; }
    if (length == 4 && ieq_range(name, 0, "CTRL", 4)) { *code = INI_CODE_CTRL; return true; }
    if (length == 3 && ieq_range(name, 0, "ALT", 3)) { *code = INI_CODE_ALT; return true; }
    if (length == 2 && ascii_toupper((char)name[0]) == 'F' &&
        name[1] >= '1' && name[1] <= '9') {
        *code = (uint8_t)(INI_CODE_F1 + (name[1] - '1')); return true;
    }
    if (length == 3 && ascii_toupper((char)name[0]) == 'F' &&
        name[1] == '1' && name[2] >= '0' && name[2] <= '2') {
        unsigned f = 10u + (unsigned)(name[2] - '0');
        *code = (uint8_t)(INI_CODE_F1 + (f - 1)); return true;
    }
    return false;
}

bool ini_config_parse(const uint8_t *data, size_t length,
                      uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT],
                      uint8_t autofire_mask[JOY_PROFILE_COUNT]) {
    bool seen[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT] = {{false}};
    bool sections[JOY_PROFILE_COUNT] = {false};
    unsigned section = JOY_PROFILE_COUNT;
    memset(autofire_mask, 0, JOY_PROFILE_COUNT);
    size_t i = 0;

    while (i < length) {
        /* Find the extent of the current line. */
        size_t ls = i;
        while (ls < length && is_space(data[ls])) ++ls;
        size_t le = ls;
        while (le < length && data[le] != '\n' && data[le] != '\r') ++le;
        i = le;
        if (i < length && data[i] == '\r') ++i;
        if (i < length && data[i] == '\n') ++i;
        if (ls == le) continue;           /* blank line */
        if (data[ls] == ';') continue;    /* comment line */

        if (data[ls] == '[' && data[le - 1] == ']') {
            size_t name_start = ls + 1;
            size_t name_length = le - ls - 2;
            section = JOY_PROFILE_COUNT;
            static const char names[JOY_PROFILE_COUNT][8] = {
                "RED", "GREEN", "PURPLE", "YELLOW"
            };
            for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
                if (name_length == strlen(names[p]) &&
                    ieq_range(data, name_start, names[p], name_length)) {
                    section = p;
                    sections[p] = true;
                    break;
                }
            }
            if (section == JOY_PROFILE_COUNT) return false;
            continue;
        }

        if (section == JOY_PROFILE_COUNT) return false;

        /* Parse "buttonN" = <value>[:AUTOFIRE]. */
        if (le - ls < 8) continue;        /* "button1=" is 8 characters */
        if (!ieq_range(data, ls, "button", 6)) continue;
        uint8_t digit = data[ls + 6];
        if (digit < '1' || digit > '4') continue;
        unsigned idx = (unsigned)(digit - '1');
        if (data[ls + 7] != '=') continue;
        size_t p = ls + 8;
        while (p < le && is_space(data[p])) ++p;

        /* Value name, up to ':' or whitespace. */
        size_t vs = p;
        while (p < le && data[p] != ':' && !is_space(data[p])) ++p;
        size_t ve = p;
        if (ve == vs) return false;       /* empty value */

        bool autofire_here = false;
        if (p < le && data[p] == ':') {
            size_t q = p + 1;
            while (q < le && is_space(data[q])) ++q;
            size_t as = q;
            while (q < le && !is_space(data[q])) ++q;
            if (q - as == 8 && ieq_range(data, as, "AUTOFIRE", 8)) {
                autofire_here = true;
                p = q;
            } else {
                return false;             /* malformed :suffix */
            }
        }

        /* Only trailing whitespace (or a comment) may follow the value. */
        while (p < le && is_space(data[p])) ++p;
        if (p < le && data[p] != ';') return false;

        uint8_t code;
        if (!name_to_code(data + vs, ve - vs, &code)) return false;
        seen[section][idx] = true;
        codes[section][idx] = code;
        if (autofire_here) autofire_mask[section] |= (uint8_t)(1u << idx);
    }

    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        if (!sections[p]) return false;
        for (unsigned k = 0; k < JOY_BUTTON_COUNT; ++k) {
            if (!seen[p][k]) return false;
        }
    }
    return true;
}

static const char *const code_names[INI_CODE_COUNT] = {
    "NONE",
    "JOY1", "JOY2", "JOY3", "JOY4",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "ENTER", "ESC", "BACKSPACE", "TAB", "SPACE",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "SHIFT", "CTRL", "ALT"
};

const char *ini_config_code_name(uint8_t code) {
    return code < INI_CODE_COUNT ? code_names[code] : "NONE";
}

uint8_t ini_config_joy_button(uint8_t code) {
    return (code >= INI_CODE_JOY1 && code <= INI_CODE_JOY4) ? code : 0;
}

uint8_t ini_config_keycode(uint8_t code) {
    if (code >= INI_CODE_A && code <= INI_CODE_A + 25) {
        return (uint8_t)(0x04 + (code - INI_CODE_A));      /* A=0x04 .. Z=0x1D */
    }
    if (code >= INI_CODE_0 && code <= INI_CODE_0 + 9) {
        return (uint8_t)(0x1e + (code - INI_CODE_0));      /* 0=0x1E .. 9=0x27 */
    }
    switch (code) {
        case INI_CODE_ENTER:     return 0x28;
        case INI_CODE_ESC:       return 0x29;
        case INI_CODE_BACKSPACE: return 0x2a;
        case INI_CODE_TAB:       return 0x2b;
        case INI_CODE_SPACE:     return 0x2c;
        default: break;
    }
    if (code >= INI_CODE_F1 && code <= INI_CODE_F1 + 11) {
        return (uint8_t)(0x3a + (code - INI_CODE_F1));     /* F1=0x3A .. F12=0x45 */
    }
    return 0;
}

uint8_t ini_config_modifier(uint8_t code) {
    switch (code) {
        case INI_CODE_SHIFT: return 0x02;  /* Left Shift */
        case INI_CODE_CTRL:  return 0x01;  /* Left Control */
        case INI_CODE_ALT:   return 0x04;  /* Left Alt */
        default:             return 0x00;
    }
}
