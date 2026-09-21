#include "ini_config.h"

#include <string.h>
#include <stdio.h>

static char ascii_toupper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool ieq_range(const uint8_t *data, size_t offset, const char *literal,
                      size_t length) {
    for (size_t i = 0; i < length; ++i)
        if (ascii_toupper((char)data[offset + i]) != ascii_toupper(literal[i])) return false;
    return true;
}

static bool is_space(uint8_t c) { return c == ' ' || c == '\t'; }

uint8_t ini_config_keycode(uint8_t code);

static bool name_to_code(const uint8_t *name, size_t length, uint8_t *code) {
    if (length == 4 && ieq_range(name, 0, "NONE", 4)) { *code = INI_CODE_NONE; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY1", 4)) { *code = INI_CODE_JOY1; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY2", 4)) { *code = INI_CODE_JOY2; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY3", 4)) { *code = INI_CODE_JOY3; return true; }
    if (length == 4 && ieq_range(name, 0, "JOY4", 4)) { *code = INI_CODE_JOY4; return true; }
    if (length == 1) {
        char c = ascii_toupper((char)name[0]);
        if (c >= 'A' && c <= 'Z') { *code = INI_CODE_A + (uint8_t)(c - 'A'); return true; }
        if (c >= '0' && c <= '9') { *code = INI_CODE_0 + (uint8_t)(c - '0'); return true; }
    }
    if (length == 5 && ieq_range(name, 0, "ENTER", 5)) { *code = INI_CODE_ENTER; return true; }
    if (length == 3 && ieq_range(name, 0, "ESC", 3)) { *code = INI_CODE_ESC; return true; }
    if (length == 9 && ieq_range(name, 0, "BACKSPACE", 9)) { *code = INI_CODE_BACKSPACE; return true; }
    if (length == 3 && ieq_range(name, 0, "TAB", 3)) { *code = INI_CODE_TAB; return true; }
    if (length == 5 && ieq_range(name, 0, "SPACE", 5)) { *code = INI_CODE_SPACE; return true; }
    if (length == 5 && ieq_range(name, 0, "SHIFT", 5)) { *code = INI_CODE_SHIFT; return true; }
    if (length == 4 && ieq_range(name, 0, "CTRL", 4)) { *code = INI_CODE_CTRL; return true; }
    if (length == 3 && ieq_range(name, 0, "ALT", 3)) { *code = INI_CODE_ALT; return true; }
    if (length == 2 && ascii_toupper((char)name[0]) == 'F' && name[1] >= '1' && name[1] <= '9') {
        *code = INI_CODE_F1 + (uint8_t)(name[1] - '1'); return true;
    }
    if (length == 3 && ascii_toupper((char)name[0]) == 'F' && name[1] == '1' && name[2] >= '0' && name[2] <= '2') {
        *code = INI_CODE_F1 + (uint8_t)(9 + name[2] - '0'); return true;
    }
    return false;
}

static bool axis_name(const uint8_t *name, size_t length, uint8_t *axis) {
    static const char names[JOY_DIRECTION_COUNT][6] = {"UP", "DOWN", "LEFT", "RIGHT"};
    for (unsigned i = 0; i < JOY_DIRECTION_COUNT; ++i) {
        if (length == strlen(names[i]) && ieq_range(name, 0, names[i], length)) {
            *axis = (uint8_t)i;
            return true;
        }
    }
    return false;
}

static bool parse_binding_value(const uint8_t *value, size_t length,
                                bool allow_autofire, ini_binding_t *binding) {
    memset(binding, 0, sizeof(*binding));
    binding->type = INI_BIND_NONE;
    size_t end = length;
    while (end && is_space(value[end - 1])) --end;
    if (end >= 9 && value[end - 9] == ':' && ieq_range(value, end - 8, "AUTOFIRE", 8)) {
        if (!allow_autofire) return false;
        binding->autofire = 1;
        end -= 9;
        while (end && is_space(value[end - 1])) --end;
    } else {
        const char *suffix = ":AUTOFIRE:";
        const size_t suffix_len = 10;
        size_t suffix_start = end;
        while (suffix_start && value[suffix_start - 1] >= '0' && value[suffix_start - 1] <= '9') --suffix_start;
        if (suffix_start >= suffix_len && value[suffix_start - suffix_len] == ':' &&
            ieq_range(value, suffix_start - suffix_len + 1, suffix + 1, suffix_len - 1)) {
            if (!allow_autofire || suffix_start == end) return false;
            uint32_t delay = 0;
            for (size_t i = suffix_start; i < end; ++i) {
                delay = delay * 10u + (uint32_t)(value[i] - '0');
                if (delay > JOY_AUTOFIRE_MAX_DELAY_MS) return false;
            }
            if (delay == 0) return false;
            binding->autofire = 1;
            binding->autofire_delay_ms = (uint16_t)delay;
            end = suffix_start - suffix_len;
            while (end && is_space(value[end - 1])) --end;
        }
    }
    if (!end) return false;

    uint8_t axis;
    if (axis_name(value, end, &axis)) {
        binding->type = INI_BIND_AXIS;
        binding->value = axis;
        return true;
    }
    uint8_t direct_code;
    if (name_to_code(value, end, &direct_code) &&
        direct_code >= INI_CODE_JOY1 && direct_code <= INI_CODE_JOY4) {
        binding->type = INI_BIND_GAMEPAD;
        binding->value = (uint8_t)(direct_code - INI_CODE_JOY1 + 1);
        return true;
    }
    uint8_t none_code;
    if (name_to_code(value, end, &none_code) && none_code == INI_CODE_NONE) {
        if (binding->autofire) return false;
        return true;
    }

    uint8_t modifier = 0;
    uint8_t keycode = INI_CODE_NONE;
    size_t start = 0;
    bool found_token = false;
    while (start < end) {
        while (start < end && is_space(value[start])) ++start;
        size_t stop = start;
        while (stop < end && value[stop] != '+') ++stop;
        while (stop > start && is_space(value[stop - 1])) --stop;
        if (stop == start) return false;
        uint8_t code;
        if (!name_to_code(value + start, stop - start, &code)) return false;
        if (code == INI_CODE_SHIFT || code == INI_CODE_CTRL || code == INI_CODE_ALT) {
            uint8_t bit = ini_config_modifier(code);
            if (modifier & bit) return false;
            modifier |= bit;
        } else {
            if (found_token || code == INI_CODE_NONE) return false;
            keycode = code;
            found_token = true;
        }
        start = stop;
        while (start < end && is_space(value[start])) ++start;
        if (start < end) {
            if (value[start] != '+') return false;
            ++start;
        }
    }
    if (!found_token && modifier == 0) return false;
    if (keycode == INI_CODE_NONE && modifier == 0) {
        binding->type = INI_BIND_NONE;
    } else {
        binding->type = INI_BIND_KEYBOARD;
        binding->value = keycode;
        binding->modifier = modifier;
        if (keycode != INI_CODE_NONE && ini_config_keycode(keycode) == 0) return false;
    }
    return true;
}

bool ini_config_parse(const uint8_t *data, size_t length,
                      ini_binding_t bindings[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT]) {
    bool seen[JOY_PROFILE_COUNT][JOY_PROFILE_INPUT_COUNT] = {{false}};
    bool sections[JOY_PROFILE_COUNT] = {false};
    unsigned section = JOY_PROFILE_COUNT;
    size_t i = 0;
    while (i < length) {
        size_t ls = i;
        while (ls < length && is_space(data[ls])) ++ls;
        size_t le = ls;
        while (le < length && data[le] != '\n' && data[le] != '\r') ++le;
        i = le;
        if (i < length && data[i] == '\r') ++i;
        if (i < length && data[i] == '\n') ++i;
        if (ls == le || data[ls] == ';') continue;
        if (data[ls] == '[' && le > ls + 1 && data[le - 1] == ']') {
            static const char names[JOY_PROFILE_COUNT][8] = {"RED", "GREEN", "BLUE", "YELLOW"};
            section = JOY_PROFILE_COUNT;
            for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
                size_t n = le - ls - 2;
                if (n == strlen(names[p]) && ieq_range(data, ls + 1, names[p], n)) {
                    section = p; sections[p] = true; break;
                }
            }
            if (section == JOY_PROFILE_COUNT) return false;
            continue;
        }
        if (section == JOY_PROFILE_COUNT) return false;
        unsigned slot = JOY_PROFILE_INPUT_COUNT;
        size_t key_start = ls;
        if (le - ls >= 7 && ieq_range(data, ls, "button", 6) && data[ls + 6] >= '1' && data[ls + 6] <= '4') {
            slot = JOY_DIRECTION_COUNT + (unsigned)(data[ls + 6] - '1');
            key_start = ls + 7;
        } else {
            static const char names[JOY_DIRECTION_COUNT][6] = {"UP", "DOWN", "LEFT", "RIGHT"};
            for (unsigned d = 0; d < JOY_DIRECTION_COUNT; ++d) {
                size_t n = strlen(names[d]);
                if (le - ls > n && ieq_range(data, ls, names[d], n) && data[ls + n] == '=') {
                    slot = d; key_start = ls + n; break;
                }
            }
        }
        if (slot >= JOY_PROFILE_INPUT_COUNT || key_start >= le || data[key_start] != '=') continue;
        size_t vs = key_start + 1;
        while (vs < le && is_space(data[vs])) ++vs;
        size_t ve = le;
        while (ve > vs && is_space(data[ve - 1])) --ve;
        size_t comment = vs;
        while (comment < ve && data[comment] != ';') ++comment;
        ve = comment;
        while (ve > vs && is_space(data[ve - 1])) --ve;
        if (!parse_binding_value(data + vs, ve - vs, true,
                                 &bindings[section][slot])) return false;
        seen[section][slot] = true;
    }
    for (unsigned p = 0; p < JOY_PROFILE_COUNT; ++p) {
        if (!sections[p]) return false;
        for (unsigned s = 0; s < JOY_PROFILE_INPUT_COUNT; ++s)
            if (!seen[p][s]) return false;
    }
    return true;
}

static const char *const code_names[INI_CODE_COUNT] = {
    "NONE", "JOY1", "JOY2", "JOY3", "JOY4",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "ENTER", "ESC", "BACKSPACE", "TAB", "SPACE",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "SHIFT", "CTRL", "ALT"
};

const char *ini_config_code_name(uint8_t code) { return code < INI_CODE_COUNT ? code_names[code] : "NONE"; }

bool ini_config_binding_format(const ini_binding_t *binding, char *out, size_t capacity) {
    const char *name = "NONE";
    if (binding->type == INI_BIND_GAMEPAD && binding->value >= 1 && binding->value <= 4) {
        name = ini_config_code_name((uint8_t)(INI_CODE_JOY1 + binding->value - 1));
    } else if (binding->type == INI_BIND_AXIS) {
        static const char *const axes[] = {"UP", "DOWN", "LEFT", "RIGHT"};
        if (binding->value >= JOY_DIRECTION_COUNT) return false;
        name = axes[binding->value];
    } else if (binding->type == INI_BIND_KEYBOARD) {
        name = ini_config_code_name(binding->value);
        if (binding->value != INI_CODE_NONE && ini_config_keycode(binding->value) == 0) return false;
    } else if (binding->type != INI_BIND_NONE) return false;
    char prefix[32] = "";
    size_t used = 0;
    if (binding->type == INI_BIND_KEYBOARD) {
        if (binding->modifier & 0x01) used += (size_t)snprintf(prefix + used, sizeof(prefix) - used, "CTRL+");
        if (binding->modifier & 0x02) used += (size_t)snprintf(prefix + used, sizeof(prefix) - used, "SHIFT+");
        if (binding->modifier & 0x04) used += (size_t)snprintf(prefix + used, sizeof(prefix) - used, "ALT+");
        if (binding->value == INI_CODE_NONE && used) prefix[used - 1] = '\0';
    }
    char suffix[32] = "";
    if (binding->autofire) {
        if (binding->autofire_delay_ms) snprintf(suffix, sizeof(suffix), ":AUTOFIRE:%u",
                                                 (unsigned)binding->autofire_delay_ms);
        else snprintf(suffix, sizeof(suffix), ":AUTOFIRE");
    }
    int n = snprintf(out, capacity, "%s%s%s", prefix, name, suffix);
    return n >= 0 && (size_t)n < capacity;
}
uint8_t ini_config_joy_button(uint8_t code) { return code >= INI_CODE_JOY1 && code <= INI_CODE_JOY4 ? code : 0; }
uint8_t ini_config_keycode(uint8_t code) {
    if (code >= INI_CODE_A && code <= INI_CODE_A + 25) return 0x04 + code - INI_CODE_A;
    if (code >= INI_CODE_0 && code <= INI_CODE_0 + 9) return 0x1e + code - INI_CODE_0;
    switch (code) {
        case INI_CODE_ENTER: return 0x28; case INI_CODE_ESC: return 0x29;
        case INI_CODE_BACKSPACE: return 0x2a; case INI_CODE_TAB: return 0x2b;
        case INI_CODE_SPACE: return 0x2c; default: break;
    }
    return code >= INI_CODE_F1 && code <= INI_CODE_F1 + 11 ? 0x3a + code - INI_CODE_F1 : 0;
}
uint8_t ini_config_modifier(uint8_t code) {
    if (code == INI_CODE_SHIFT) return 0x02;
    if (code == INI_CODE_CTRL) return 0x01;
    if (code == INI_CODE_ALT) return 0x04;
    return 0;
}
