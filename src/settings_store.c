#include "settings_store.h"

#include <string.h>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"

#define SETTINGS_SECTOR_SIZE 4096u
#define SETTINGS_FLASH_BASE (PICO_FLASH_SIZE_BYTES - 2u * SETTINGS_SECTOR_SIZE)

const joystick_settings_record_t *settings_flash_record(unsigned slot) {
    uintptr_t address = XIP_BASE + SETTINGS_FLASH_BASE + slot * SETTINGS_SECTOR_SIZE;
    return (const joystick_settings_record_t *)address;
}

typedef struct {
    uint32_t offset;
    const uint8_t *page;
} settings_flash_write_t;

static void __not_in_flash_func(settings_flash_write_callback)(void *param) {
    settings_flash_write_t *write = (settings_flash_write_t *)param;
    flash_range_erase(write->offset, SETTINGS_SECTOR_SIZE);
    flash_range_program(write->offset, write->page, FLASH_PAGE_SIZE);
}

static int settings_save_page(uint32_t offset, const uint8_t *page) {
    settings_flash_write_t write = { .offset = offset, .page = page };
    return flash_safe_execute(settings_flash_write_callback, &write, 100);
}

void settings_store_init(settings_store_t *store, unsigned active_slot,
                         uint32_t sequence) {
    *store = (settings_store_t){
        .active_slot = active_slot & 1u,
        .sequence = sequence,
    };
}

void settings_store_queue(settings_store_t *store,
                          const joystick_settings_t *settings) {
    joystick_settings_record_t record =
        joystick_settings_record_make(settings, store->sequence + 1u);
    memset(store->pending_page, 0xff, FLASH_PAGE_SIZE);
    memcpy(store->pending_page, &record, sizeof(record));
    store->pending_offset = SETTINGS_FLASH_BASE +
                            (store->active_slot ^ 1u) * SETTINGS_SECTOR_SIZE;
    store->pending = true;
}

bool settings_store_pending(const settings_store_t *store) {
    return store->pending;
}

bool settings_store_finish(settings_store_t *store) {
    if (!store->pending) return true;
    if (settings_save_page(store->pending_offset, store->pending_page) == 0) {
        store->active_slot ^= 1u;
        ++store->sequence;
        store->pending = false;
        return true;
    }
    return false;
}
