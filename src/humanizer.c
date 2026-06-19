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

void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, uint16_t smoothing_rate,
                   float* phase, float* prev_mag, float* prev_out_x, float* prev_out_y) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;
    
    float magnitude = sqrtf((x * x) + (y * y));
    *prev_mag = magnitude; 

    // --- 1. SLOW CONTINUOUS WAVE CLOCK ---
    // Drastically slowed down so it actually looks like a fluid swing
    float base_speed = 0.002f; 
    *phase += base_speed;
    if (*phase > 100000.0f) *phase -= 100000.0f;

    // --- 2. WAVEFORM SUMMATION ---
    float wave = sinf(*phase * 1.0f) + 
                 sinf(*phase * 1.37f) + 
                 sinf(*phase * 0.79f);
    wave = wave / 2.5f;

    // --- 3. LOOSE-CENTER DEFLECTION CURVE ---
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (6.0f * (M_PI / 180.0f));
            
            float deflection_ratio = magnitude / AXIS_MAX;
            if (deflection_ratio > 1.0f) deflection_ratio = 1.0f;
            
            // True Spring Physics: 
            // 1.0 at dead center (loose/high wobble), 0.0 at full edge (tight)
            float looseness = 1.0f - deflection_ratio; 
            
            // At full press, keep 10% wobble. At light touch, go up to 100% wobble.
            float curve_multiplier = 0.10f + (0.90f * looseness);
            
            angle += (wave * max_wobble_rads * curve_multiplier);
        }

        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));
        if (magnitude > limit) {
            magnitude = limit; 
        }

        x = magnitude * cosf(angle);
        y = magnitude * sinf(angle);
    }

    // --- 4. HIGH-FREQUENCY SMOOTHING (True Friction) ---
    if (smoothing_rate > 0) {
        // Maps 0-100 down to an alpha of 1.0 (instant) to 0.02 (heavy drag)
        // This is required for microcontrollers running thousands of loops a second
        float alpha = 1.0f - (smoothing_rate / 100.0f * 0.98f);

        x = *prev_out_x + alpha * (x - *prev_out_x);
        y = *prev_out_y + alpha * (y - *prev_out_y);
    }

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
    
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_l), &(h->prev_mag_l), &(h->prev_out_x_l), &(h->prev_out_y_l));
                  
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, smoothing_rate, 
                  &(h->phase_r), &(h->prev_mag_r), &(h->prev_out_x_r), &(h->prev_out_y_r));
}
