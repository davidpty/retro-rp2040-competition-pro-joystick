#ifndef MSC_DISK_H_
#define MSC_DISK_H_

#include <stdbool.h>
#include <stdint.h>

#include "joystick_types.h"
#include "msc_volume.h"

void msc_disk_init(const joystick_settings_t *settings, bool config_drive);
void msc_disk_rebuild(const joystick_settings_t *settings);
bool msc_disk_enabled(void);
bool msc_disk_ejected(void);
bool msc_disk_modified(void);
void msc_disk_ack(void);
uint32_t msc_disk_last_write_us(void);
const msc_volume_t *msc_disk_volume(void);

#endif
