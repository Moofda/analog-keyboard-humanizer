#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>

typedef struct {
    float noise_l; // Tracks the current wobble angle for the left stick
    float noise_r; // Tracks the current wobble angle for the right stick
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation);

#endif
