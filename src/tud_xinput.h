#ifndef _TUD_XINPUT_H_
#define _TUD_XINPUT_H_

#include <stdint.h>
#include <stdbool.h>

bool tud_xinput_send_report(const uint8_t *report, uint16_t len);

#endif
