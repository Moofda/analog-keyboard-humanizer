#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // Stochastic Pink Noise States (EMA)
    float tremor_state;
    float tilt_state;
    float gate_state;
    
    // 2nd-Order Physics State Variables
    float pos_lx, pos_ly;
    float vel_lx, vel_ly;
    
    bool was_active_l;
    float land_offset_l;
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry,
                       uint16_t circ_error, 
                       uint16_t jitter_mag, uint16_t jitter_inner, uint16_t jitter_outer, 
                       uint16_t smoothing_rate, uint16_t gate_level,
                       uint16_t variance_level, int16_t ergo_tilt, uint16_t landing_var, 
                       uint16_t diagonal_feel, uint16_t anti_deadzone, uint16_t passthrough);

#endif
