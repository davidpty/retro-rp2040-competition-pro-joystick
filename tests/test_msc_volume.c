#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ini_config.h"
#include "joystick.h"
#include "msc_volume.h"
#include "settings.h"

static void le16(const uint8_t *p, uint16_t *out) {
    *out = (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}

static void le32(const uint8_t *p, uint32_t *out) {
    *out = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static void le16_put(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static joystick_settings_t sample_settings(void) {
    joystick_settings_t s;
    joystick_settings_defaults(&s);
    s.button_code[0] = INI_CODE_A;
    s.button_code[1] = INI_CODE_SPACE;
    s.button_code[2] = INI_CODE_SHIFT;
    s.button_code[3] = INI_CODE_NONE;
    s.autofire_mask = 0x5; /* buttons 1 and 3 */
    joystick_settings_sync_active_profile(&s);
    return s;
}

static void test_boot_sector(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);
    uint16_t bytes_per_sector, total_sectors, root_entries, spf, spt, heads,
        reserved;
    uint8_t cluster_size;
    le16(&v.disk[0][11], &bytes_per_sector);
    assert(bytes_per_sector == 512);
    cluster_size = v.disk[0][13];
    assert(cluster_size == 1);
    le16(&v.disk[0][14], &reserved);
    le16(&v.disk[0][17], &root_entries);
    le16(&v.disk[0][19], &total_sectors);
    le16(&v.disk[0][22], &spf);
    le16(&v.disk[0][24], &spt);
    le16(&v.disk[0][26], &heads);
    assert(reserved == 1 && root_entries == 16 && spf == 1 && spt == 1 &&
           heads == 1);
    assert(v.disk[0][16] == 1 && v.disk[0][21] == 0xf8 &&
           v.disk[0][36] == 0x80 && v.disk[0][38] == 0x29);
    assert(v.disk[0][510] == 0x55 && v.disk[0][511] == 0xaa);
}

static void test_fat_and_root(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);

    /* FAT: media and reserved entries are present; the file may span clusters. */
    assert(v.disk[1][0] == 0xf8 && v.disk[1][1] == 0xff && v.disk[1][2] == 0xff);

    /* Volume label entry. */
    assert(memcmp(&v.disk[2][0], "CONFIG      ", 11) == 0);
    assert(v.disk[2][11] == 0x08);

    /* JOYSTICK.INI file entry. */
    const uint8_t *entry = &v.disk[2][32];
    assert(memcmp(entry, "JOYSTICKINI", 11) == 0);
    assert(entry[11] == 0x20);
    uint16_t cluster;
    le16(&entry[26], &cluster);
    assert(cluster == 2);
    uint32_t size;
    le32(&entry[28], &size);
    assert(size == v.ini_size && v.ini_size > 0);
}

static void test_round_trip(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);
    uint8_t data[MSC_DISK_BLOCK_SIZE * 2];
    size_t length = msc_volume_read_ini(&v, data, sizeof(data));
    assert(length == v.ini_size);
    uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT];
    uint8_t masks[JOY_PROFILE_COUNT];
    assert(ini_config_parse(data, length, codes, masks));
    for (unsigned i = 0; i < 4; ++i) assert(codes[0][i] == s.button_code[i]);
    assert(masks[0] == s.autofire_mask);
}

static void test_host_rewrite_bounded_parse(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);

    /* Simulate the host saving a shorter file, leaving stale bytes after the
     * stored size. The parse must stop at the size bound. */
    static const char new_file[] =
        "[RED]\r\nbutton1=W\r\nbutton2=X\r\nbutton3=Y\r\nbutton4=Z\r\n"
        "[GREEN]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n"
        "[PURPLE]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n"
        "[YELLOW]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n";
    const size_t new_len = strlen(new_file);
    assert(new_len < MSC_DISK_BLOCK_SIZE);
    memcpy(v.disk[3], new_file, new_len);
    /* Stale bytes from the old content beyond the new size. */
    memcpy(&v.disk[3][new_len], "button1=SPACE\r\n", 15);
    le16_put(&v.disk[2][32 + 28], (uint16_t)new_len);

    uint8_t data[MSC_DISK_BLOCK_SIZE * 2];
    size_t length = msc_volume_read_ini(&v, data, sizeof(data));
    assert(length == new_len);
    uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT];
    uint8_t masks[JOY_PROFILE_COUNT];
    assert(ini_config_parse(data, length, codes, masks));
    assert(codes[0][0] == INI_CODE_A + 22 && codes[0][1] == INI_CODE_A + 23 &&
           codes[0][2] == INI_CODE_A + 24 && codes[0][3] == INI_CODE_A + 25);
    assert(masks[0] == 0);
}

static void test_host_relocated_save(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);

    /* Simulate an atomic save (temp file + rename): the rewritten file lands
     * in a fresh cluster. The original cluster 2 is freed, cluster 3 becomes
     * the file's start and its chain ends there. The reader must follow the
     * directory entry to the relocated content instead of the stale disk[3]. */
    static const char new_file[] =
        "[RED]\r\nbutton1=SPACE:AUTOFIRE\r\nbutton2=JOY1\r\nbutton3=JOY2\r\nbutton4=W\r\n"
        "[GREEN]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n"
        "[PURPLE]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n"
        "[YELLOW]\r\nbutton1=A\r\nbutton2=B\r\nbutton3=C\r\nbutton4=D\r\n";
    const size_t new_len = strlen(new_file);
    assert(new_len < MSC_DISK_BLOCK_SIZE);
    memset(v.disk[3], 0, MSC_DISK_BLOCK_SIZE);
    memcpy(v.disk[4], new_file, new_len);
    /* FAT: cluster 2 free, cluster 3 end of chain. */
    const uint8_t fat[] = { 0xf8, 0xff, 0xff, 0x00, 0xf0, 0xff };
    memcpy(v.disk[1], fat, sizeof(fat));
    uint8_t *entry = &v.disk[2][32];
    le16_put(&entry[26], 3);
    le16_put(&entry[28], (uint16_t)new_len);

    uint8_t data[MSC_DISK_BLOCK_SIZE * 2];
    size_t length = msc_volume_read_ini(&v, data, sizeof(data));
    assert(length == new_len);
    uint8_t codes[JOY_PROFILE_COUNT][JOY_BUTTON_COUNT];
    uint8_t masks[JOY_PROFILE_COUNT];
    assert(ini_config_parse(data, length, codes, masks));
    assert(codes[0][0] == INI_CODE_SPACE && codes[0][1] == INI_CODE_JOY1 &&
           codes[0][2] == INI_CODE_JOY2 && codes[0][3] == INI_CODE_A + 22);
    assert(masks[0] == (1u << 0));
}

static void test_unusable_entry_fallback(void) {
    msc_volume_t v;
    joystick_settings_t s = sample_settings();
    msc_volume_rebuild(&v, &s);
    /* Corrupt the file entry: fall back to the full data sector. */
    memset(&v.disk[2][32], 0, 32);
    uint8_t data[MSC_DISK_BLOCK_SIZE * 2];
    size_t length = msc_volume_read_ini(&v, data, sizeof(data));
    assert(length == MSC_DISK_BLOCK_SIZE);
}

int test_msc_volume_main(void) {
    test_boot_sector();
    test_fat_and_root();
    test_round_trip();
    test_host_rewrite_bounded_parse();
    test_host_relocated_save();
    test_unusable_entry_fallback();
    puts("msc volume tests passed");
    return 0;
}
