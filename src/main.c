#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "tusb.h"
#include "xinput_host.h"
#include "humanizer.h"

// Adafruit Feather RP2040 USB Host pins
#define USB_HOST_DP_PIN  16
#define USB_HOST_DM_PIN  17
#define USB_HOST_PWR_PIN 18

// XInput report structure
typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  report_size;
    uint8_t  buttons1;
    uint8_t  buttons2;
    uint8_t  trigger_l;
    uint8_t  trigger_r;
    int16_t  stick_lx;
    int16_t  stick_ly;
    int16_t  stick_rx;
    int16_t  stick_ry;
    uint8_t  reserved[6];
} XInputReport;

// XInput output report
typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t report_size;
    uint8_t rumble_l;
    uint8_t rumble_r;
    uint8_t reserved[3];
} XInputOutReport;

static Humanizer humanizer;
static XInputReport current_report = {0};
static bool report_ready = false;

//--------------------------------------------------------------------
// Core 1 - USB Host (reads keyboard)
//--------------------------------------------------------------------
void core1_main(void)
{
    // Enable USB host power
    gpio_init(USB_HOST_PWR_PIN);
    gpio_set_dir(USB_HOST_PWR_PIN, GPIO_OUT);
    gpio_put(USB_HOST_PWR_PIN, 1);

    tuh_init(BOARD_TUH_RHPORT);

    while (true) {
        tuh_task();
    }
}

//--------------------------------------------------------------------
// XInput host callbacks
//--------------------------------------------------------------------
void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                    xinput_gamepad_t const* report,
                                    uint16_t len)
{
    (void)dev_addr; (void)instance; (void)len;

    current_report.report_id  = 0x00;
    current_report.report_size = 0x14;
    current_report.buttons1   = (report->wButtons) & 0xFF;
    current_report.buttons2   = (report->wButtons >> 8) & 0xFF;
    current_report.trigger_l  = report->bLeftTrigger;
    current_report.trigger_r  = report->bRightTrigger;
    current_report.stick_lx   = report->sThumbLX;
    current_report.stick_ly   = report->sThumbLY;
    current_report.stick_rx   = report->sThumbRX;
    current_report.stick_ry   = report->sThumbRY;

    // Apply humanization
    humanizer_process(&humanizer,
        &current_report.stick_lx, &current_report.stick_ly,
        &current_report.stick_rx, &current_report.stick_ry);

    report_ready = true;
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance,
                          const xinput_gamepad_t* pad)
{
    (void)dev_addr; (void)instance; (void)pad;
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr; (void)instance;
    report_ready = false;
}

//--------------------------------------------------------------------
// TinyUSB device callbacks (XInput output to PC)
//--------------------------------------------------------------------
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                 hid_report_type_t report_type,
                                 uint8_t* buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type;
    if (reqlen >= sizeof(XInputReport)) {
        memcpy(buffer, &current_report, sizeof(XInputReport));
        return sizeof(XInputReport);
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                             hid_report_type_t report_type,
                             uint8_t const* buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)bufsize;
}

//--------------------------------------------------------------------
// Main
//--------------------------------------------------------------------
int main(void)
{
    stdio_init_all();

    // Initialize humanizer
    humanizer_init(&humanizer);

    // Initialize USB device (XInput output to PC)
    tud_init(BOARD_TUD_RHPORT);

    // Launch USB host on core 1
    multicore_launch_core1(core1_main);

    while (true) {
        tud_task();

        // Send report to PC when ready
        if (report_ready && tud_vendor_mounted()) {
            tud_vendor_write(&current_report, sizeof(XInputReport));
            tud_vendor_flush();
            report_ready = false;
        }
    }

    return 0;
}
