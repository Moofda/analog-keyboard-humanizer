#include "humanizer.h"
#include <math.h>
#include <stdlib.h>

#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->current_noise_l = 0.0f;
    h->target_noise_l = 0.0f;
    h->current_noise_r = 0.0f;
    h->target_noise_r = 0.0f;
}

// Internal helper to apply math to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, 
                   float* current_noise, float* target_noise) {
    
    // --- 1. THE 24/7 NOISE ENGINE ---
    // Runs constantly so instant presses inherit a random starting offset!
    if (deviation_level > 0) {
        // If we reached our current random target, pick a new one (-1.0 to 1.0)
        if (fabsf(*target_noise - *current_noise) < 0.05f) {
            *target_noise = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
        }
        // Smoothly sweep current noise toward the target
        // The multiplier (0.005f) controls the speed of the thumb wander
        *current_noise = (*current_noise) + ((*target_noise - *current_noise) * 0.005f);
    } else {
        *current_noise = 0.0f;
        *target_noise = 0.0f;
    }

    float x = (float)raw_x;
    float y = (float)raw_y;
    float magnitude = sqrtf((x * x) + (y * y));
    
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        // --- 2. APPLY THE SWEEPING WOBBLE ---
        if (deviation_level > 0) {
            // Scale slider (0-100) to max wobble degrees (e.g., up to +/- 6 degrees)
            float max_wobble_rads = (deviation_level / 100.0f) * (6.0f * (M_PI / 180.0f));
            angle += (*current_noise * max_wobble_rads);
        }

        // --- 3. GATE CIRCULARITY ---
        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));
        if (magnitude > limit) {
            magnitude = limit; 
        }

        // Convert back to X/Y
        x = magnitude * cosf(angle);
        y = magnitude * sinf(angle);
    }

    // Hardware failsafe limits
    if (x > AXIS_MAX) x = AXIS_MAX;
    if (x < -AXIS_MAX) x = -AXIS_MAX;
    if (y > AXIS_MAX) y = AXIS_MAX;
    if (y < -AXIS_MAX) y = -AXIS_MAX;

    *out_x = (int16_t)x;
    *out_y = (int16_t)y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation) {
    
    // Pass the state memory pointers into the processor
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, &(h->current_noise_l), &(h->target_noise_l));
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, &(h->current_noise_r), &(h->target_noise_r));
}
