#ifndef SETTINGS_STORE_H_
#define SETTINGS_STORE_H_

#include <stdbool.h>
#include <stdint.h>

#include "hardware/flash.h"
#include "settings.h"

typedef struct {
    unsigned active_slot;
    uint32_t sequence;
    bool pending;
    uint32_t pending_offset;
    uint8_t pending_page[FLASH_PAGE_SIZE];
} settings_store_t;

const joystick_settings_record_t *settings_flash_record(unsigned slot);
void settings_store_init(settings_store_t *store, unsigned active_slot,
                         uint32_t sequence);
void settings_store_queue(settings_store_t *store,
                          const joystick_settings_t *settings);
bool settings_store_pending(const settings_store_t *store);
bool settings_store_finish(settings_store_t *store);

#endif
