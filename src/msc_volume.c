#include "msc_volume.h"

#include <string.h>

#include "ini_config.h"

#define MSC_VOLUME_LABEL    "CONFIG"
#define MSC_FILE_NAME       "JOYSTICK"
#define MSC_FILE_EXT        "INI"
#define MSC_FILE_CLUSTER    2u  /* First data cluster (LBA 3). */
#define MSC_ROOT_ENTRIES    16u
#define MSC_FAT_SECTORS     1u
#define MSC_RESERVED        1u
#define MSC_DATA_START_LBA  (MSC_RESERVED + MSC_FAT_SECTORS + \
                             MSC_ROOT_ENTRIES * 32u / MSC_DISK_BLOCK_SIZE)

static void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void build_boot_sector(uint8_t block[MSC_DISK_BLOCK_SIZE]) {
    memset(block, 0, MSC_DISK_BLOCK_SIZE);
    block[0] = 0xeb; block[1] = 0x3c; block[2] = 0x90;
    memcpy(&block[3], "MSDOS5.0", 8);
    put_le16(&block[11], MSC_DISK_BLOCK_SIZE);      /* bytes per sector */
    block[13] = 1;                                  /* sectors per cluster */
    put_le16(&block[14], MSC_RESERVED);             /* reserved sectors */
    block[16] = 1;                                  /* number of FATs */
    put_le16(&block[17], MSC_ROOT_ENTRIES);         /* root entries */
    put_le16(&block[19], MSC_DISK_BLOCK_NUM);       /* total sectors (16-bit) */
    block[21] = 0xf8;                               /* media descriptor */
    put_le16(&block[22], MSC_FAT_SECTORS);          /* sectors per FAT */
    put_le16(&block[24], 1);                        /* sectors per track */
    put_le16(&block[26], 1);                        /* number of heads */
    block[36] = 0x80;                               /* drive number */
    block[38] = 0x29;                               /* extended boot signature */
    put_le32(&block[39], 0x1234u);                  /* volume serial number */
    memcpy(&block[43], MSC_VOLUME_LABEL, strlen(MSC_VOLUME_LABEL));
    block[49] = ' ';
    memcpy(&block[54], "FAT12   ", 8);
    block[510] = 0x55;
    block[511] = 0xaa;
}

static void build_fat(uint8_t block[MSC_DISK_BLOCK_SIZE], unsigned clusters) {
    memset(block, 0, MSC_DISK_BLOCK_SIZE);
    block[0] = 0xf8; block[1] = 0xff; block[2] = 0xff;
    for (unsigned cluster = 2; cluster < 2 + clusters; ++cluster) {
        uint16_t next = cluster + 1 < 2 + clusters ? (uint16_t)(cluster + 1) : 0x0fffu;
        unsigned offset = cluster + cluster / 2;
        if ((cluster & 1u) == 0) {
            block[offset] = (uint8_t)next;
            block[offset + 1] = (uint8_t)((block[offset + 1] & 0xf0u) |
                                          (next >> 8));
        } else {
            block[offset] = (uint8_t)((block[offset] & 0x0fu) | (next << 4));
            block[offset + 1] = (uint8_t)(next >> 4);
        }
    }
}

static void build_root_directory(uint8_t block[MSC_DISK_BLOCK_SIZE],
                                 uint16_t ini_size) {
    memset(block, 0, MSC_DISK_BLOCK_SIZE);
    /* Volume label entry. */
    memcpy(&block[0], MSC_VOLUME_LABEL, strlen(MSC_VOLUME_LABEL));
    for (unsigned i = strlen(MSC_VOLUME_LABEL); i < 11; ++i) block[i] = ' ';
    block[11] = 0x08;

    /* JOYSTICK.INI file entry ("JOYSTICKINI", no dot). */
    uint8_t *entry = &block[32];
    memcpy(entry, MSC_FILE_NAME, strlen(MSC_FILE_NAME));
    memcpy(&entry[8], MSC_FILE_EXT, 3);
    entry[11] = 0x20;              /* archive */
    entry[14] = 0x52; entry[15] = 0x6d;   /* create time */
    entry[16] = 0x65; entry[17] = 0x43;   /* create date */
    entry[18] = 0x65; entry[19] = 0x43;   /* last access date */
    entry[22] = 0x88; entry[23] = 0x6d;   /* write time */
    entry[24] = 0x65; entry[25] = 0x43;   /* write date */
    put_le16(&entry[26], MSC_FILE_CLUSTER);
    put_le32(&entry[28], ini_size);
}

static size_t format_ini(const joystick_settings_t *settings,
                         uint8_t *out, size_t cap) {
    static const char header[] =
        "; Retro RP2040 Competition Pro configuration\r\n"
        "; Each direction and button accepts one output, or NONE to disable it.\r\n"
        "; Joystick: UP DOWN LEFT RIGHT. Gamepad: JOY1 JOY2 JOY3 JOY4.\r\n"
        "; Keyboard: A-Z, 0-9, ENTER ESC BACKSPACE TAB SPACE, or F1-F12.\r\n"
        "; Modifiers: SHIFT CTRL ALT. Combine with +, for example SHIFT+A.\r\n"
        "; Add :AUTOFIRE to any output, for example JOY1:AUTOFIRE or UP:AUTOFIRE.\r\n"
        "; Reverse axes by swapping values, for example up=DOWN and down=UP.\r\n"
        "\r\n";
    static const char key[][12] = { "button1=", "button2=", "button3=", "button4=",
                                    "up=", "down=", "left=", "right=" };
    static const char section[][12] = { "[RED]\r\n", "[BLUE]\r\n",
                                        "[GREEN]\r\n", "[YELLOW]\r\n" };
    static const uint8_t profile_order[] = { 0, 2, 1, 3 };
    size_t used = 0;
    size_t hlen = sizeof(header) - 1;
    if (used + hlen > cap) return 0;
    memcpy(out + used, header, hlen);
    used += hlen;
    for (unsigned output = 0; output < JOY_PROFILE_COUNT; ++output) {
        unsigned p = profile_order[output];
        size_t section_len = strlen(section[output]);
        if (used + section_len > cap) return 0;
        memcpy(out + used, section[output], section_len);
        used += section_len;
        for (unsigned i = 0; i < JOY_PROFILE_INPUT_COUNT; ++i) {
            const ini_binding_t *binding = i < JOY_BUTTON_COUNT
                ? &settings->profiles[p].button[i]
                : &settings->profiles[p].direction[i - JOY_BUTTON_COUNT];
            char value[64];
            size_t key_len = strlen(key[i]);
            if (!ini_config_binding_format(binding, value, sizeof(value))) return 0;
            size_t value_len = strlen(value);
            if (used + key_len + value_len + 2 > cap) return 0;
            memcpy(out + used, key[i], key_len); used += key_len;
            memcpy(out + used, value, value_len); used += value_len;
            out[used++] = '\r';
            out[used++] = '\n';
        }
        if (used + 2 > cap) return 0;
        out[used++] = '\r';
        out[used++] = '\n';
    }
    return used;
}

void msc_volume_rebuild(msc_volume_t *volume, const joystick_settings_t *settings) {
    build_boot_sector(volume->disk[0]);
    uint8_t content[MSC_DISK_BLOCK_NUM * MSC_DISK_BLOCK_SIZE];
    size_t size = format_ini(settings, content, sizeof(content));
    unsigned clusters = (unsigned)((size + MSC_DISK_BLOCK_SIZE - 1) /
                                   MSC_DISK_BLOCK_SIZE);
    if (clusters == 0) clusters = 1;
    build_fat(volume->disk[1], clusters);
    build_root_directory(volume->disk[2], 0);  /* size patched after content */
    for (unsigned lba = MSC_DATA_START_LBA; lba < MSC_DISK_BLOCK_NUM; ++lba) {
        size_t offset = (lba - MSC_DATA_START_LBA) * MSC_DISK_BLOCK_SIZE;
        memset(volume->disk[lba], 0, MSC_DISK_BLOCK_SIZE);
        if (offset < size) {
            size_t count = size - offset;
            if (count > MSC_DISK_BLOCK_SIZE) count = MSC_DISK_BLOCK_SIZE;
            memcpy(volume->disk[lba], content + offset, count);
        }
    }
    volume->ini_size = (uint16_t)size;
    build_root_directory(volume->disk[2], (uint16_t)size);
}

/* FAT12 entry for a cluster, read from the FAT in LBA 1. End-of-chain values
 * are 0xff8..0xfff; 0xff7 is a bad cluster and 0x000 means free. */
static uint16_t fat12_next(const msc_volume_t *volume, uint16_t cluster) {
    const uint8_t *fat = volume->disk[1];
    uint16_t e = (uint16_t)(cluster + (cluster >> 1));
    uint16_t value = (cluster & 1)
                         ? (uint16_t)((fat[e] >> 4) | (uint16_t)fat[e + 1] << 4)
                         : (uint16_t)(fat[e] | ((uint16_t)(fat[e + 1] & 0x0f) << 8));
    return (uint16_t)(value & 0x0fffu);
}

size_t msc_volume_read_ini(const msc_volume_t *volume, uint8_t *out, size_t cap) {
    const uint8_t *entry = &volume->disk[2][32];
    static const char want[] = "JOYSTICKINI";
    bool match = memcmp(entry, want, 11) == 0 && entry[11] == 0x20;
    size_t copied = 0;

    if (!match) {
        /* Unusable entry: fall back to the legacy single data sector. */
        size_t n = MSC_DISK_BLOCK_SIZE < cap ? MSC_DISK_BLOCK_SIZE : cap;
        memcpy(out, volume->disk[MSC_DATA_START_LBA], n);
        return n;
    }

    uint32_t size = (uint32_t)entry[28] | (uint32_t)entry[29] << 8 |
                    (uint32_t)entry[30] << 16 | (uint32_t)entry[31] << 24;
    uint16_t cluster = (uint16_t)(entry[26] | (uint16_t)entry[27] << 8);
    if (size == 0) {
        /* A zero-size entry during an in-progress save: expose the whole data
         * sector so the content trailing the stored size is still parseable. */
        size = MSC_DISK_BLOCK_SIZE;
    }
    unsigned steps = 0;
    while (copied < size && copied < cap && steps < 14u) {
        if (cluster < 2u) break;
        uint32_t lba = MSC_DATA_START_LBA + (uint32_t)(cluster - 2u);
        if (lba >= MSC_DISK_BLOCK_NUM) break;
        const uint8_t *sector = volume->disk[lba];
        size_t n = size - copied;
        if (n > MSC_DISK_BLOCK_SIZE) n = MSC_DISK_BLOCK_SIZE;
        if (n > cap - copied) n = cap - copied;
        memcpy(out + copied, sector, n);
        copied += n;
        if (copied >= size || copied >= cap) break;
        uint16_t next = fat12_next(volume, cluster);
        if (next < 2u) break;
        cluster = next;
        ++steps;
    }
    return copied;
}
