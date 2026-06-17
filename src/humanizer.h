#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>

typedef struct {
    float current_noise_l; 
    float target_noise_l;  
    float prev_mag_l;      // PHASE 2.5: Tracks release velocity for the left stick
    
    float current_noise_r;
    float target_noise_r;
    float prev_mag_r;      // PHASE 2.5: Tracks release velocity for the right stick
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation);

#endif
