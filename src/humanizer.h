#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>

typedef struct {
    float phase_l;    
    float prev_mag_l; 
    float prev_out_x_l; // PHASE 3 MEMORY: Left X
    float prev_out_y_l; // PHASE 3 MEMORY: Left Y
    
    float phase_r;    
    float prev_mag_r; 
    float prev_out_x_r; // PHASE 3 MEMORY: Right X
    float prev_out_y_r; // PHASE 3 MEMORY: Right Y
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation, uint16_t smoothing_rate);

#endif
