#ifndef CONFIG_SERVICE_H_
#define CONFIG_SERVICE_H_

#include "joystick_types.h"

typedef enum {
    CONFIG_APPLY_REJECTED,
    CONFIG_APPLY_UNCHANGED,
    CONFIG_APPLY_CHANGED,
} config_apply_result_t;

config_apply_result_t config_service_apply(const joystick_settings_t *current);

#endif
