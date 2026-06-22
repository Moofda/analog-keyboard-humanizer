#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float wobble_phase;
    float tilt_phase;
    float gate_phase;
    
    // 2nd-Order Physics State Variables
    float pos_lx, pos_ly;
    float vel_lx, vel_ly;
    float pos_rx, pos_ry;
    float vel_rx, vel_ry;
    
    bool was_active_l;
    float land_offset_l;
    bool was_active_r;
    float land_offset_r;
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry,
                       uint16_t circ_error, 
                       uint16_t jitter_mag, uint16_t jitter_inner, uint16_t jitter_outer, 
                       uint16_t smoothing_rate, uint16_t gate_level,
                       uint16_t tilt_deg, uint16_t landing_var, 
                       uint16_t diagonal_feel, // <-- The new dial
                       uint16_t passthrough);

#endif // HUMANIZER_H
