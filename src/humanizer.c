#include "humanizer.h"
#include <math.h>

#define AXIS_MAX 32767.0f 
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void humanizer_init(Humanizer* h) {
    h->phase_l = 0.0f;
    h->prev_x_l = 0.0f;
    h->prev_y_l = 0.0f;
    
    h->phase_r = 0.0f;
    h->prev_x_r = 0.0f;
    h->prev_y_r = 0.0f;
}

// Notice: lerp_angle is GONE. We don't need it anymore!

void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, uint16_t smoothing_rate,
                   float* phase, float* prev_x, float* prev_y) {
    
    // ==========================================
    // STEP 1: CARTESIAN SMOOTHING (The Slingshot Killer)
    // Smoothing X and Y directly guarantees straight lines.
    // ==========================================
    float alpha = 1.0f;
    if (smoothing_rate > 0) {
        alpha = 1.0f - (smoothing_rate / 100.0f * 0.98f);
    }

    float sm_x = *prev_x + alpha * ((float)raw_x - *prev_x);
    float sm_y = *prev_y + alpha * ((float)raw_y - *prev_y);
    
    // Save state for the next frame
    *prev_x = sm_x;
    *prev_y = sm_y;

    // ==========================================
    // STEP 2: CONVERT TO POLAR FOR STEALTH MODS
    // ==========================================
    float mag = sqrtf((sm_x * sm_x) + (sm_y * sm_y));
    float angle = 0.0f;
    
    if (mag > 0.0f) {
        angle = atan2f(sm_y, sm_x);

        // --- 1. CLOCK ---
        *phase += 0.002f;
        if (*phase > 100000.0f) *phase -= 100000.0f;

        // --- 2. WAVEFORM SUMMATION ---
        float wave = sinf(*phase * 1.0f) + 
                     sinf(*phase * 1.37f) + 
                     sinf(*phase * 0.79f);
        wave = wave / 2.5f;

        // --- 3. AXIS ROTATION (Ergonomic Tilt) ---
        angle += -5.0f * (M_PI / 180.0f);

        // --- 4. THE WOBBLE & EDGE-LIFT ---
        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (20.0f * (M_PI / 180.0f));
            
            float deflection_ratio = mag / AXIS_MAX;
            if (deflection_ratio > 1.0f) deflection_ratio = 1.0f;
            
            float looseness = 1.0f - deflection_ratio; 
            float curve_multiplier = 0.25f + (0.75f * looseness);
            
            angle += (wave * max_wobble_rads * curve_multiplier);
        }

        // --- 5. THE BREATHING OUTER RING ---
        float breathing_wave = sinf(*phase * 0.8f);
        float breathing_scaler = 0.5f + (0.5f * breathing_wave);
        float dynamic_max_gate = AXIS_MAX * (1.0f - (0.015f * breathing_scaler));

        // --- 6. CIRCULARITY ERROR ---
        float limit = dynamic_max_gate * (1.0f + (error_pct / 100.0f));
        if (mag > limit) {
            mag = limit; 
        }
    }

    // ==========================================
    // STEP 3: CONVERT BACK TO HARDWARE OUTPUT
    // ==========================================
    float final_x = mag * cosf(angle);
    float final_y = mag * sinf(angle);

    if (final_x > AXIS_MAX) final_x = AXIS_MAX;
    if (final_x < -AXIS_MAX) final_x = -AXIS_MAX;
    if (final_y > AXIS_MAX) final_y = AXIS_MAX;
    if (final_y < -AXIS_MAX) final_y = -AXIS_MAX;

    *out_x = (int16_t)final_x;
    *out_y = (int16_t)final_y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation, uint16_t smoothing_rate) {
    
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_l), &(h->prev_x_l), &(h->prev_y_l));
                  
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_r), &(h->prev_x_r), &(h->prev_y_r));
}
