#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_MCU           OPT_MCU_RP2040
#define CFG_TUSB_OS            OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_RHPORT        0

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN     __attribute__((aligned(4)))

#define CFG_TUD_ENABLED        1
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_HID            2
#define CFG_TUD_HID_EP_BUFSIZE 16

#define CFG_TUD_MSC            1
#define CFG_TUD_MSC_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif