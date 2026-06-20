#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>

typedef struct {
    float phase_l;    
    float prev_x_l; 
    float prev_y_l;   
    
    float phase_r;    
    float prev_x_r; 
    float prev_y_r; 
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation, uint16_t smoothing_rate);

#endif
