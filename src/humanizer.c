#include "humanizer.h"
#include <math.h>

#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->phase_l = 0.0f;
    h->prev_mag_l = 0.0f;
    h->prev_out_x_l = 0.0f;
    h->prev_out_y_l = 0.0f;
    
    h->phase_r = 0.0f;
    h->prev_mag_r = 0.0f;
    h->prev_out_x_r = 0.0f;
    h->prev_out_y_r = 0.0f;
}

// Internal helper to apply math to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, uint16_t smoothing_rate,
                   float* phase, float* prev_mag, float* prev_out_x, float* prev_out_y) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;
    
    float magnitude = sqrtf((x * x) + (y * y));
    float mag_delta = magnitude - *prev_mag;
    *prev_mag = magnitude; 

    // --- 1. CONTINUOUS WAVE CLOCK ---
    float base_speed = 0.08f; 
    if (mag_delta < -10.0f) {
        float speed_boost = fabsf(mag_delta) / 1000.0f; 
        if (speed_boost > 0.5f) speed_boost = 0.5f; 
        base_speed += speed_boost;
    }
    *phase += base_speed;
    if (*phase > 100000.0f) *phase -= 100000.0f;

    // --- 2. WAVEFORM SUMMATION ---
    float wave = sinf(*phase * 1.0f) + 
                 sinf(*phase * 1.37f) + 
                 sinf(*phase * 0.79f);
    wave = wave / 2.5f;

    // --- 3. APPLY TO ANGLE WITH DEFLECTION CURVE ---
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (6.0f * (M_PI / 180.0f));
            float deflection_ratio = magnitude / AXIS_MAX;
            if (deflection_ratio > 1.0f) deflection_ratio = 1.0f;
            float curve_multiplier = 0.20f + (0.80f * (deflection_ratio * deflection_ratio));
            angle += (wave * max_wobble_rads * curve_multiplier);
        }

        // Gate Circularity (Phase 1)
        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));
        if (magnitude > limit) {
            magnitude = limit; 
        }

        x = magnitude * cosf(angle);
        y = magnitude * sinf(angle);
    }

    // --- PHASE 3: DEFLECTION SMOOTHING (The Friction Filter) ---
    if (smoothing_rate > 0) {
        float alpha = 1.0f - (smoothing_rate / 100.0f);
        if (alpha < 0.05f) alpha = 0.05f; 

        // We pull the NEW calculated target (x, y) toward the TRUE previous output
        x = *prev_out_x + alpha * (x - *prev_out_x);
        y = *prev_out_y + alpha * (y - *prev_out_y);
    }

    // Save the finalized output into memory for the NEXT frame to use!
    *prev_out_x = x;
    *prev_out_y = y;

    // Hardware failsafe limits
    if (x > AXIS_MAX) x = AXIS_MAX;
    if (x < -AXIS_MAX) x = -AXIS_MAX;
    if (y > AXIS_MAX) y = AXIS_MAX;
    if (y < -AXIS_MAX) y = -AXIS_MAX;

    *out_x = (int16_t)x;
    *out_y = (int16_t)y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation, uint16_t smoothing_rate) {
    
    // Pass the new memory pointers into the process function
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_l), &(h->prev_mag_l), &(h->prev_out_x_l), &(h->prev_out_y_l));
                  
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_r), &(h->prev_mag_r), &(h->prev_out_x_r), &(h->prev_out_y_r));
}
