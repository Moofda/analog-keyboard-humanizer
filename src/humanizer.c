#include "humanizer.h"
#include <math.h>
#include <stdlib.h>

#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->noise_l = 0.0f;
    h->noise_r = 0.0f;
}

// Internal helper to apply math to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, float* noise_state) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;

    float magnitude = sqrtf((x * x) + (y * y));
    
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        // --- PHASE 2: AXIS DEVIATION (Smooth Rotational Noise) ---
        if (deviation_level > 0) {
            // Pick a random target between -1.0 and +1.0
            float random_target = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
            
            // Low-Pass Filter: Pull the current noise state 2% toward the target every millisecond
            *noise_state = (*noise_state) + ((random_target - *noise_state) * 0.02f);
            
            // Scale the slider (0-100) to a max wobble of +/- 5 degrees
            float max_wobble_rads = (deviation_level / 100.0f) * (5.0f * (M_PI / 180.0f));
            
            // Warp the true angle
            angle += (*noise_state * max_wobble_rads);
        }

        // --- PHASE 1: GATE CIRCULARITY ---
        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));
        if (magnitude > limit) {
            magnitude = limit; // Clamp it to the circular gate
        }

        // Convert the warped angle and clamped magnitude back to X/Y coordinates
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
    
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, &(h->noise_l));
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, &(h->noise_r));
}
