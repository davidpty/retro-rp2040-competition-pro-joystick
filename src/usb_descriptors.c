#include "tusb.h"
#include "usb_descriptors.h"
#include "config.h"
#include "pico/unique_id.h"
#include <string.h>

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = JOY_USB_VID,
    .idProduct          = JOY_USB_PID,
    .bcdDevice          = JOY_USB_BCD_DEVICE,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

/* Two signed 8-bit axes followed by three buttons and five padding bits. */
uint8_t const desc_hid_report[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
        HID_USAGE(HID_USAGE_DESKTOP_X),
        HID_USAGE(HID_USAGE_DESKTOP_Y),
        HID_LOGICAL_MIN_N(-127, 2),
        HID_LOGICAL_MAX_N(127, 2),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(2),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),
        HID_USAGE_MIN(1),
        HID_USAGE_MAX(3),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_SIZE(1),
        HID_REPORT_COUNT(3),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        HID_REPORT_SIZE(5),
        HID_REPORT_COUNT(1),
        HID_INPUT(HID_CONSTANT),
    HID_COLLECTION_END
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

enum { ITF_NUM_HID, ITF_NUM_TOTAL };
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          0, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), CFG_TUD_HID_EP_IN,
                       CFG_TUD_HID_EP_BUFSIZE, 1)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

enum { STRID_LANGID, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL };
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    JOY_USB_MANUFACTURER,
    JOY_USB_PRODUCT,
    NULL
};
static char serial_desc[32];
static uint16_t desc_str[32 + 1];

static void build_serial_string(void) {
    serial_desc[0] = '\0';
    pico_get_unique_board_id_string(serial_desc, sizeof(serial_desc));
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t count;
    if (index == STRID_LANGID) {
        desc_str[1] = 0x0409;
        count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        if (index == STRID_SERIAL && serial_desc[0] == '\0') build_serial_string();
        char const *s = index == STRID_PRODUCT ? JOY_USB_PRODUCT :
                        (index == STRID_SERIAL ? serial_desc : string_desc_arr[index]);
        count = strlen(s);
        if (count > 32) count = 32;
        for (size_t i = 0; i < count; ++i) desc_str[1 + i] = (uint8_t)s[i];
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return desc_str;
}
