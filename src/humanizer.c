#include "humanizer.h"
#include <math.h>

#define AXIS_MAX 32767.0f 
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void humanizer_init(Humanizer* h) {
    h->phase_l = 0.0f;
    h->prev_raw_mag_l = 0.0f;
    h->prev_out_mag_l = 0.0f;
    h->prev_out_angle_l = 0.0f;
    
    h->phase_r = 0.0f;
    h->prev_raw_mag_r = 0.0f;
    h->prev_out_mag_r = 0.0f;
    h->prev_out_angle_r = 0.0f;
}

// A special math helper to ensure the angle always takes the shortest, natural path 
float lerp_angle(float current, float target, float alpha) {
    float diff = target - current;
    while (diff > M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;
    
    float new_angle = current + (alpha * diff);
    
    while (new_angle > M_PI) new_angle -= 2.0f * M_PI;
    while (new_angle < -M_PI) new_angle += 2.0f * M_PI;
    
    return new_angle;
}

void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, uint16_t smoothing_rate,
                   float* phase, float* prev_raw_mag, float* prev_out_mag, float* prev_out_angle) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;
    
    float raw_mag = sqrtf((x * x) + (y * y));
    *prev_raw_mag = raw_mag; 

    // --- 1. CLOCK ---
    float base_speed = 0.002f; 
    *phase += base_speed;
    if (*phase > 100000.0f) *phase -= 100000.0f;

    // --- 2. WAVEFORM SUMMATION ---
    float wave = sinf(*phase * 1.0f) + 
                 sinf(*phase * 1.37f) + 
                 sinf(*phase * 0.79f);
    wave = wave / 2.5f;

    // We isolate the TARGET math from the SMOOTHING math
    float target_mag = raw_mag;
    float target_angle = *prev_out_angle; 

    if (raw_mag > 0) {
        target_angle = atan2f(y, x);

        // ==========================================
        // SPRINT 1: AXIS ROTATION (ERGONOMIC TILT)
        // ==========================================
        // Permanently shifts baseline by -5.0 degrees
        float rotation_offset = -5.0f * (M_PI / 180.0f);
        target_angle += rotation_offset;
        
        // Keep bounded
        if (target_angle > M_PI) target_angle -= 2.0f * M_PI;
        if (target_angle < -M_PI) target_angle += 2.0f * M_PI;

        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (20.0f * (M_PI / 180.0f));
            
            float deflection_ratio = raw_mag / AXIS_MAX;
            if (deflection_ratio > 1.0f) deflection_ratio = 1.0f;
            
            float looseness = 1.0f - deflection_ratio; 
            // Sprint 1 Edge-Wobble Floor Lift (Already implemented by you!)
            float curve_multiplier = 0.25f + (0.75f * looseness);
            
            target_angle += (wave * max_wobble_rads * curve_multiplier);
        }

        // ==========================================
        // SPRINT 1: THE BREATHING OUTER RING
        // ==========================================
        // Uses your existing phase clock. At 0.002 base speed per frame, 
        // sinf(*phase * 0.8f) yields a nice slow 15-second breathing cycle.
        float breathing_wave = sinf(*phase * 0.8f);
        float breathing_scaler = 0.5f + (0.5f * breathing_wave);
        float dynamic_max_gate = AXIS_MAX * (1.0f - (0.015f * breathing_scaler));

        // Phase 1 Circularity applied to the new breathing gate
        float limit = dynamic_max_gate * (1.0f + (error_pct / 100.0f));
        if (target_mag > limit) {
            target_mag = limit; 
        }
    }

    // --- 3. POLAR SMOOTHING (The true analog fix) ---
    float alpha = 1.0f;
    if (smoothing_rate > 0) {
        alpha = 1.0f - (smoothing_rate / 100.0f * 0.98f);
    }

    // Smooth the Magnitude (Radius)
    float new_mag = *prev_out_mag + alpha * (target_mag - *prev_out_mag);
    
    // Smooth the Angle (The sweep/roll fix)
    float new_angle = *prev_out_angle;
    if (new_mag > 0.0f) { 
        new_angle = lerp_angle(*prev_out_angle, target_angle, alpha);
    }

    // Save Polar State
    *prev_out_mag = new_mag;
    *prev_out_angle = new_angle;

    // --- 4. CONVERT POLAR BACK TO X/Y ---
    x = new_mag * cosf(new_angle);
    y = new_mag * sinf(new_angle);

    if (x > AXIS_MAX) x = AXIS_MAX;
    if (x < -AXIS_MAX) x = -AXIS_MAX;
    if (y > AXIS_MAX) y = AXIS_MAX;
    if (y < -AXIS_MAX) y = -AXIS_MAX;

    *out_x = (int16_t)x;
    *out_y = (int16_t)y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error, uint16_t axis_deviation, uint16_t smoothing_rate) {
    
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_l), &(h->prev_raw_mag_l), &(h->prev_out_mag_l), &(h->prev_out_angle_l));
                  
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_r), &(h->prev_raw_mag_r), &(h->prev_out_mag_r), &(h->prev_out_angle_r));
}
