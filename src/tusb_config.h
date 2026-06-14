#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_TUD_RHPORT      0
#define BOARD_TUH_RHPORT      1

#define CFG_TUSB_MCU          OPT_MCU_RP2040
#define CFG_TUSB_OS           OPT_OS_PICO
#define CFG_TUSB_DEBUG        0

// Device - XInput output to PC
#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 8

// Disable all device classes except vendor
#define CFG_TUD_HID           0
#define CFG_TUD_CDC           0
#define CFG_TUD_MSC           0
#define CFG_TUD_MIDI          0
#define CFG_TUD_AUDIO         0
#define CFG_TUD_VIDEO         0
#define CFG_TUD_DFU_RUNTIME   0
#define CFG_TUD_VENDOR        1
#define CFG_TUD_VENDOR_RX_BUFSIZE 64
#define CFG_TUD_VENDOR_TX_BUFSIZE 64

// Host - XInput keyboard
#define CFG_TUH_ENABLED       1
#define CFG_TUH_RPI_PIO_USB   1
#define CFG_TUH_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#define CFG_TUH_XINPUT        1
#define CFG_TUH_DEVICE_MAX    1
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HID           0
#define CFG_TUH_CDC           0
#define CFG_TUH_MSC           0
#define CFG_TUH_HUB           0

#ifdef __cplusplus
}
#endif

#endif
