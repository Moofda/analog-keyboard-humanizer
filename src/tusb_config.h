#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// RHPort number used for device
#define BOARD_TUD_RHPORT      0
#define BOARD_TUH_RHPORT      1

// Common config
#define CFG_TUSB_MCU          OPT_MCU_RP2040
#define CFG_TUSB_OS           OPT_OS_PICO
#define CFG_TUSB_DEBUG        0

// Device config
#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64

// Device classes
#define CFG_TUD_HID           0
#define CFG_TUD_CDC           0
#define CFG_TUD_MSC           0
#define CFG_TUD_MIDI          0
#define CFG_TUD_VENDOR        1

// Host config
#define CFG_TUH_ENABLED       1
#define CFG_TUH_RPI_PIO_USB   1
#define CFG_TUH_MAX_SPEED     OPT_MODE_DEFAULT_SPEED

// XInput host
#define CFG_TUH_XINPUT        1
#define CFG_TUH_DEVICE_MAX    1
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// Host classes
#define CFG_TUH_HID           0
#define CFG_TUH_CDC           0
#define CFG_TUH_MSC           0
#define CFG_TUH_HUB           0

#ifdef __cplusplus
}
#endif

#endif
