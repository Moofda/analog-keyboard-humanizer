#include "tusb.h"
#include "device/usbd_pvt.h"
#include "tud_xinput.h"
#include <string.h>

#define ENDPOINT_SIZE 20  

// ====================================================================
// 1. THE OFFICIAL XBOX 360 DEVICE DESCRIPTOR
// ====================================================================
static const uint8_t desc_device[] = {
    0x12,       // bLength
    0x01,       // bDescriptorType (Device)
    0x00, 0x02, // bcdUSB 2.00
    0xFF,       // bDeviceClass (Vendor Specific)
    0xFF,       // bDeviceSubClass
    0xFF,       // bDeviceProtocol
    0x08,       // bMaxPacketSize0 (Strictly 8 bytes)
    0x5E, 0x04, // idVendor 0x045E (Microsoft)
    0x8E, 0x02, // idProduct 0x028E (Official Xbox 360 Controller ID for instant native binding)
    0x14, 0x01, // bcdDevice 0x0114
    0x01,       // iManufacturer
    0x02,       // iProduct
    0x03,       // iSerialNumber
    0x01,       // bNumConfigurations
};

uint8_t const * tud_descriptor_device_cb(void)
{
    return desc_device;
}

// ====================================================================
// 2. THE CORRECTED 49-BYTE CONFIGURATION DESCRIPTOR
// ====================================================================
static const uint8_t desc_configuration[] = {
    // Configuration Header (Total Length: 49 Bytes -> 0x31)
    0x09, 0x02, 0x31, 0x00, 0x01, 0x01, 0x00, 0xA0, 0xFA,

    // Interface 0 (Gamepad Subclass 0x5D, Protocol 0x01)
    0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,
    
    // The Critical Xbox 360 Handshake Block (17 Bytes)
    0x11, 0x21, 0x00, 0x01, 0x01, 0x25, 0x81, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x08, 0x00, 0x00,
    
    // Endpoint 1: IN (Data to PC)
    0x07, 0x05, 0x81, 0x03, 0x20, 0x00, 0x04,
    
    // Endpoint 2: OUT (Data from PC)
    0x07, 0x05, 0x02, 0x03, 0x20, 0x00, 0x08
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

// ====================================================================
// 3. STRING DESCRIPTORS
// ====================================================================
static const char* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // Language ID
    "Microsoft Corporation",       // Manufacturer
    "Xbox 360 Controller",         // Product
    "000000000001"                 // Serial
};

static uint16_t _desc_str[128];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;
        const char* str = string_desc_arr[index];
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 127) chr_count = 127;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2*chr_count + 2));
    return _desc_str;
}

// ====================================================================
// 4. CLASS DRIVER LOGIC
// ====================================================================
static uint8_t endpoint_in  = 0xFF;
static uint8_t endpoint_out = 0xFF;
static uint8_t ep_in_buffer[ENDPOINT_SIZE];
static uint8_t ep_out_buffer[ENDPOINT_SIZE];

static void xinput_init(void)
{
    endpoint_in  = 0xFF;
    endpoint_out = 0xFF;
    memset(ep_out_buffer, 0, ENDPOINT_SIZE);
    memset(ep_in_buffer,  0, ENDPOINT_SIZE);
}

static bool xinput_deinit(void)
{
    xinput_init();
    return true;
}

static void xinput_reset(uint8_t rhport)
{
    (void)rhport;
    xinput_init();
}

static uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_length)
{
    uint16_t driver_length = sizeof(tusb_desc_interface_t) + (itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t)) + 17;

    TU_VERIFY(max_length >= driver_length, 0);

    uint8_t const *cur_desc = tu_desc_next(itf_desc);
    uint8_t found = 0;

    while (found < itf_desc->bNumEndpoints && driver_length <= max_length)
    {
        tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *)cur_desc;
        if (TUSB_DESC_ENDPOINT == tu_desc_type(ep_desc))
        {
            TU_ASSERT(usbd_edpt_open(rhport, ep_desc));
            if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN)
                endpoint_in = ep_desc->bEndpointAddress;
            else
                endpoint_out = ep_desc->bEndpointAddress;
            found++;
        }
        cur_desc = tu_desc_next(cur_desc);
    }

    if (endpoint_out != 0xFF)
        usbd_edpt_xfer(BOARD_TUD_RHPORT, endpoint_out, ep_out_buffer, ENDPOINT_SIZE);

    return driver_length;
}

static bool xinput_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    (void)rhport; (void)stage; (void)request;
    return true;
}

static bool xinput_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)rhport; (void)result; (void)xferred_bytes;
    if (ep_addr == endpoint_out)
        usbd_edpt_xfer(BOARD_TUD_RHPORT, endpoint_out, ep_out_buffer, ENDPOINT_SIZE);
    return true;
}

static const usbd_class_driver_t xinput_driver =
{
#if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
#else
    .name = NULL,
#endif
    .init             = xinput_init,
    .deinit           = xinput_deinit,
    .reset            = xinput_reset,
    .open             = xinput_open,
    .control_xfer_cb  = xinput_control_xfer_cb,
    .xfer_cb          = xinput_xfer_cb,
    .sof              = NULL
};

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &xinput_driver;
}

bool tud_xinput_send_report(const uint8_t *report, uint16_t len)
{
    if (!tud_ready() || endpoint_in == 0xFF) return false;
    if (usbd_edpt_busy(BOARD_TUD_RHPORT, endpoint_in)) return false;
    if (len > ENDPOINT_SIZE) len = ENDPOINT_SIZE;
    memcpy(ep_in_buffer, report, len);
    usbd_edpt_claim(BOARD_TUD_RHPORT, endpoint_in);
    usbd_edpt_xfer(BOARD_TUD_RHPORT, endpoint_in, ep_in_buffer, len);
    usbd_edpt_release(BOARD_TUD_RHPORT, endpoint_in);
    return true;
}
