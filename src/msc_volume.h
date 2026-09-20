#ifndef MSC_VOLUME_H_
#define MSC_VOLUME_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "joystick.h"

#define MSC_DISK_BLOCK_NUM  16u
#define MSC_DISK_BLOCK_SIZE 512u

typedef struct {
    uint8_t disk[MSC_DISK_BLOCK_NUM][MSC_DISK_BLOCK_SIZE];
    uint16_t ini_size;
} msc_volume_t;

void msc_volume_rebuild(msc_volume_t *volume, const joystick_settings_t *settings);

/* Read the JOYSTICK.INI file content from the on-disk image, following the
 * root-directory entry's start cluster through the FAT12 chain. Copies up to
 * the entry's stored file size (or cap, whichever is smaller) into out and
 * returns the number of bytes copied. Stale trailing bytes past the stored
 * size are never exposed. If the entry is unusable or its cluster chain is
 * missing, the first (legacy) data sector is copied so single-cluster saves
 * still parse. */
size_t msc_volume_read_ini(const msc_volume_t *volume, uint8_t *out, size_t cap);

#endif