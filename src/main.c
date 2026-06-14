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
#define USB_HOST_PWR_PIN 18

// XInput report to send to PC
static uint8_t current_report[20] = {0};
static bool report_ready = false;

static Humanizer humanizer;

//--------------------------------------------------------------------
// Core 1 - USB Host
//--------------------------------------------------------------------
void core1_main(void)
{
    gpio_init(USB_HOST_PWR_PIN);
    gpio_set_dir(USB_HOST_PWR_PIN, GPIO_OUT);
    gpio_put(USB_HOST_PWR_PIN, 1);

    tuh_init(BOARD_TUH_RHPORT);

    while (true) {
        tuh_task();
    }
}

//--------------------------------------------------------------------
// XInput host callbacks - use correct tusb_xinput signatures
//--------------------------------------------------------------------
void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                    xinputh_interface_t const* xid_itf,
                                    uint16_t len)
{
    (void)dev_addr; (void)instance; (void)len;

    const xinput_gamepad_t* p = &xid_itf->pad;

    int16_t lx = p->sThumbLX;
    int16_t ly = p->sThumbLY;
    int16_t rx = p->sThumbRX;
    int16_t ry = p->sThumbRY;

    humanizer_process(&humanizer, &lx, &ly, &rx, &ry);

    current_report[0]  = 0x00;
    current_report[1]  = 0x14;
    current_report[2]  = (p->wButtons) & 0xFF;
    current_report[3]  = (p->wButtons >> 8) & 0xFF;
    current_report[4]  = p->bLeftTrigger;
    current_report[5]  = p->bRightTrigger;
    current_report[6]  = lx & 0xFF;
    current_report[7]  = (lx >> 8) & 0xFF;
    current_report[8]  = ly & 0xFF;
    current_report[9]  = (ly >> 8) & 0xFF;
    current_report[10] = rx & 0xFF;
    current_report[11] = (rx >> 8) & 0xFF;
    current_report[12] = ry & 0xFF;
    current_report[13] = (ry >> 8) & 0xFF;

    report_ready = true;
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance,
                          const xinputh_interface_t* xinput_itf)
{
    (void)dev_addr; (void)instance; (void)xinput_itf;
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr; (void)instance;
    report_ready = false;
    memset(current_report, 0, sizeof(current_report));
}

//--------------------------------------------------------------------
// Main
//--------------------------------------------------------------------
int main(void)
{
    stdio_init_all();

    humanizer_init(&humanizer);

    tud_init(BOARD_TUD_RHPORT);

    multicore_launch_core1(core1_main);

    while (true) {
        tud_task();

        if (report_ready && tud_vendor_mounted()) {
            tud_vendor_write(current_report, sizeof(current_report));
            tud_vendor_flush();
            report_ready = false;
        }
    }

    return 0;
}
