#include "humanizer.h"
#include <math.h>

#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->phase_l = 0.0f;
    h->prev_mag_l = 0.0f;
    
    h->phase_r = 0.0f;
    h->prev_mag_r = 0.0f;
}

// Internal helper to apply math to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, 
                   uint16_t error_pct, uint16_t deviation_level, 
                   float* phase, float* prev_mag) {
    
    float x = (float)raw_x;
    float y = (float)raw_y;
    
    float magnitude = sqrtf((x * x) + (y * y));
    float mag_delta = magnitude - *prev_mag;
    *prev_mag = magnitude; 

    // --- 1. CONTINUOUS WAVE CLOCK ---
    // The "phase" is basically a stopwatch running in the background.
    float base_speed = 0.08f; 

    // Phase 2.5: The Spring Release Turbo!
    // We kept this, but now it accelerates the SINE WAVE frequency instead of random static
    if (mag_delta < -10.0f) {
        float speed_boost = fabsf(mag_delta) / 1000.0f; 
        if (speed_boost > 0.5f) speed_boost = 0.5f; 
        base_speed += speed_boost;
    }

    // Always advance the clock so the global preset keeps breathing
    *phase += base_speed;

    // Prevent the float from overflowing if left plugged in for weeks
    if (*phase > 100000.0f) *phase -= 100000.0f;

    // --- 2. WAVEFORM SUMMATION ---
    // We sum 3 sine waves operating at unaligned prime-number frequencies (1.0, 1.37, 0.79).
    // This creates a smooth, rolling, pendulum-like wave that almost never repeats exactly.
    float wave = sinf(*phase * 1.0f) + 
                 sinf(*phase * 1.37f) + 
                 sinf(*phase * 0.79f);
                 
    // Divide by 2.5 to roughly normalize the sum back down to a -1.0 to +1.0 range
    wave = wave / 2.5f;


    // --- 3. APPLY TO ANGLE WITH DEFLECTION CURVE ---
    if (magnitude > 0) {
        float angle = atan2f(y, x);

        if (deviation_level > 0) {
            float max_wobble_rads = (deviation_level / 100.0f) * (6.0f * (M_PI / 180.0f));
            
            // Calculate how far the stick is pushed (0.0 to 1.0)
            float deflection_ratio = magnitude / AXIS_MAX;
            if (deflection_ratio > 1.0f) deflection_ratio = 1.0f;

            // THE HYBRID CURVE: 20% Global Baseline + 80% Quadratic Deflection Scaling
            // Light taps = stable and tight. Heavy presses = wild and sweeping.
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
    
    process_stick(lx, ly, *lx, *ly, circ_error, axis_deviation, &(h->phase_l), &(h->prev_mag_l));
    process_stick(rx, ry, *rx, *ry, circ_error, axis_deviation, &(h->phase_r), &(h->prev_mag_r));
}
