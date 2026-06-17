#include "humanizer.h"
#include <math.h>
#include <stdlib.h>

#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->current_noise_l = 0.0f;
    h->target_noise_l = 0.0f;
    h->prev_mag_l = 0.0f;
    
    h->current_noise_r = 0.0f;
    h->target_noise_r = 0.0f;
    h->prev_mag_r = 0.0f;
}

// Internal helper to apply math to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, 
                   float* current_noise, float* target_noise, float* prev_mag) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;
    
    // Calculate current magnitude and velocity (delta)
    float magnitude = sqrtf((x * x) + (y * y));
    float mag_delta = magnitude - *prev_mag;
    *prev_mag = magnitude; // Save state for the next frame

    // --- 1. THE VELOCITY-AWARE NOISE ENGINE ---
    if (deviation_level > 0) {
        if (fabsf(*target_noise - *current_noise) < 0.05f) {
            *target_noise = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
        }

        // Base sweep speed tuned for a 71Hz USB polling rate (Holding/Pressing)
        float sweep_speed = 0.03f;

        // PHASE 2.5: Spring Release Divergence
        // If magnitude drops by more than 100 units in a single frame, the key is physically rising.
        // We turbo-charge the wobble to mimic spring tension snap-back!
        if (mag_delta < -100.0f) {
            sweep_speed = 0.25f; 
        }

        *current_noise = (*current_noise) + ((*target_noise - *current_noise) * sweep_speed);
    } else {
        *current_noise = 0.0f;
        *target_noise = 0.0f;
    }

    // --- 2. APPLY ROTATION AND CLAMP ---
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (6.0f * (M_PI / 180.0f));
            angle += (*current_noise * max_wobble_rads);
        }

        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));
        if (magnitude > limit) {
            magnitude = limit; 
        }

        x = magnitude * cosf(angle);
        y = magnitude * sinf(angle);
    }

    if (x > AXIS_MAX) x = AXIS_MAX;
    if (x < -AXIS_MAX) x = -AXIS_MAX;
    if (y > AXIS_MAX) y = AXIS_MAX;
    if (y < -AXIS_MAX) y = -AXIS_MAX;

    *out_x = (int16_t)x;
    *out_y = (int16_t)y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation) {
    
    // Pass the state memory pointers (including prev_mag) into the processor
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, 
                  &(h->current_noise_l), &(h->target_noise_l), &(h->prev_mag_l));
                  
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, 
                  &(h->current_noise_r), &(h->target_noise_r), &(h->prev_mag_r));
}
