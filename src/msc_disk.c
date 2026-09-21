#include "msc_disk.h"

#include <string.h>

#include "pico/stdlib.h"
#include "tusb.h"

static msc_volume_t volume;
static bool enabled;
static bool ejected;
static bool modified;
static bool mounted;
static uint32_t last_write_us;

void msc_disk_init(const joystick_settings_t *settings, bool config_drive) {
    enabled = config_drive;
    ejected = false;
    modified = false;
    mounted = false;
    last_write_us = 0;
    if (enabled) msc_volume_rebuild(&volume, settings);
}

void msc_disk_rebuild(const joystick_settings_t *settings) {
    if (!enabled) return;
    msc_volume_rebuild(&volume, settings);
    ejected = false;
    modified = false;
    last_write_us = 0;
}

bool msc_disk_enabled(void) {
    return enabled;
}

bool msc_disk_ejected(void) {
    return ejected;
}

bool msc_disk_modified(void) {
    return modified;
}

uint32_t msc_disk_last_write_us(void) {
    return last_write_us;
}

void msc_disk_ack(void) {
    /* An apply attempt consumed the pending writes/eject; arm the drive for
     * the next change without ending config mode. */
    modified = false;
    ejected = false;
}

void tud_mount_cb(void) {
    if (enabled) mounted = true;
}

void tud_umount_cb(void) {
    if (enabled && mounted) {
        mounted = false;
        ejected = true;
    }
}

const msc_volume_t *msc_disk_volume(void) {
    return &volume;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    static const char vid[] = "Joystick";
    static const char pid[] = "Config";
    static const char rev[] = "1.0";
    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    if (ejected || !enabled) {
        /* Additional Sense 3A-00 is NOT_FOUND. */
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    *block_count = MSC_DISK_BLOCK_NUM;
    *block_size = MSC_DISK_BLOCK_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    if (load_eject) {
        if (start) {
            ejected = false;
        } else {
            /* Host requested an unmount/eject; apply pending changes now. */
            ejected = true;
        }
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    (void)lun;
    if (lba >= MSC_DISK_BLOCK_NUM) return -1;
    memcpy(buffer, &volume.disk[lba][offset], bufsize);
    return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return enabled;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    if (lba >= MSC_DISK_BLOCK_NUM) return -1;
    memcpy(&volume.disk[lba][offset], buffer, bufsize);
    modified = true;
    last_write_us = time_us_32();
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {
    (void)lun; (void)scsi_cmd; (void)buffer; (void)bufsize;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}
