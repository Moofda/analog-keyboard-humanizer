#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h" 
#include "hardware/structs/watchdog.h"

#include "pio_usb.h"         
#include "tusb.h"
#include "xinput_host.h"
#include "tud_xinput.h"
#include "humanizer.h"

// Forward declarations to link custom functions inside tud_xinput.c
void tud_set_config_mode(bool enable);
bool tud_in_config_mode(void);
void tud_config_handle_serial(void);

#define USB_HOST_PWR_PIN 18
#define CONFIG_MAGIC_NUM 0x1A2B3C4D 

static uint8_t current_report[20] = {0};
static volatile bool report_ready = false; 
static Humanizer humanizer;

// Caching variables for the continuous state-machine background scanner
static volatile uint16_t latest_buttons = 0;
static uint32_t combo_start_time = 0;

// ====================================================================
// CORE 1: EXCLUSIVE USB HOST CONTROLLER & STATE SCANNER
// ====================================================================
void core1_main(void)
{
    gpio_init(USB_HOST_PWR_PIN);
    gpio_set_dir(USB_HOST_PWR_PIN, GPIO_OUT);
    gpio_put(USB_HOST_PWR_PIN, 1);

    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = 16; 
    
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);
    
    while (true) {
        tuh_task();
        
        // CONTINUOUS BACKGROUND STATE SCANNER (Matches OGX-Mini's design)
        // Keeps ticking even if the USB cable goes completely quiet during a stationary hold
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // COMBO MASK: Start (0x0010) + LB (0x0100) + RB (0x0200) = 0x0310
        if ((latest_buttons & 0x0310) == 0x0310) {
            if (combo_start_time == 0) {
                combo_start_time = now; // Set the baseline anchor time
            } else if (now - combo_start_time >= 3000) {
                // Pinched successfully for 3 absolute seconds!
                watchdog_hw->scratch[0] = CONFIG_MAGIC_NUM;
                watchdog_reboot(0, 0, 10);
                while(1);
            }
        } else {
            combo_start_time = 0; // Clear immediately if any button slips
        }
    }
}

usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
{
    *driver_count = 1;
    return &usbh_xinput_driver;
}

// ====================================================================
// HOST DEVICE EVENT CALLBACKS
// ====================================================================
void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t* xinput_itf)
{
    (void)dev_addr; (void)instance; (void)xinput_itf;
    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr; (void)instance;
    report_ready = false;
    latest_buttons = 0; // Reset cache if the keyboard is disconnected
    combo_start_time = 0;
    memset(current_report, 0, sizeof(current_report));
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf, uint16_t len)
{
    (void)dev_addr; (void)instance; (void)len;
    const xinput_gamepad_t* p = &xid_itf->pad;
    
    // Cache the absolute latest raw button data instantly for Core 1's scanner thread
    latest_buttons = p->wButtons;

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
    
    tuh_xinput_receive_report(dev_addr, instance);
}

// ====================================================================
// CORE 0: PC DEVICE EMULATION LOOP
// ====================================================================
int main(void)
{
    set_sys_clock_khz(120000, true);
    stdio_init_all();
    humanizer_init(&humanizer);
    
    if (watchdog_hw->scratch[0] == CONFIG_MAGIC_NUM) {
        watchdog_hw->scratch[0] = 0;
        tud_set_config_mode(true);
    } else {
        tud_set_config_mode(false);
    }
    
    tud_init(BOARD_TUD_RHPORT);
    multicore_launch_core1(core1_main);
    
    while (true) {
        tud_task();
        
        if (tud_in_config_mode()) {
            tud_config_handle_serial();
        } else if (report_ready) {
            tud_xinput_send_report(current_report, sizeof(current_report));
            report_ready = false;
        }
    }
    return 0;
}
