#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// Struct to hold stateful memory for the physics engine
typedef struct {
    // Oscillators for organic breathing
    float wobble_phase;
    float tilt_phase;
    float gate_phase;
    
    // EMA (Exponential Moving Average) Smoothing state
    float ema_lx, ema_ly;
    float ema_rx, ema_ry;

    // Landing Hysteresis (Dice Roll) state
    bool was_active_l;
    float land_offset_l;
    
    bool was_active_r;
    float land_offset_r;
} Humanizer;

// Initialize the engine memory
void humanizer_init(Humanizer* h);

// The core physics loop
void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry,
                       uint16_t circ_error, 
                       uint16_t jitter_mag, uint16_t jitter_inner, uint16_t jitter_outer, 
                       uint16_t smoothing_rate, uint16_t gate_level,
                       uint16_t tilt_deg, uint16_t landing_var, uint16_t passthrough);

#endif // HUMANIZER_H
