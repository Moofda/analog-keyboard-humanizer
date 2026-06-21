#include "humanizer.h"
#include <math.h>
#include "pico/time.h"

#define AXIS_MAX 32767.0f
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// reference tick the walk is tuned against; dt scaling normalizes real rate to this
#define REF_HZ 250.0f

static inline float frand(Humanizer* h) {
    // xorshift32 -> [-1, 1)
    h->rng ^= h->rng << 13;
    h->rng ^= h->rng >> 17;
    h->rng ^= h->rng << 5;
    return ((float)(h->rng & 0xFFFFFF) / (float)0x800000) - 1.0f;
}

// cheap central-limit gaussian approx
static inline float gauss(Humanizer* h) {
    return (frand(h) + frand(h) + frand(h)) * 0.5f;
}

void humanizer_init(Humanizer* h) {
    h->prev_x_l = h->prev_y_l = 0.0f;
    h->prev_x_r = h->prev_y_r = 0.0f;
    
    // Initialize Physics State
    h->wob_p_l = 0.0f; h->wob_v_l = 0.0f;
    h->wob_p_r = 0.0f; h->wob_v_r = 0.0f;
    
    h->bias_l = h->bias_r = 0.0f;
    h->gate_l = h->gate_r = 0.0f;
    h->sig_l = h->sig_r = 1.0f;
    
    h->last_us = 0;
    h->have_last = 0;
    h->rng = 0xC0FFEEu;
}

static void process_stick(Humanizer* h,
                          int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y,
                          uint16_t error_pct, uint16_t deviation_level,
                          uint16_t smoothing_rate, uint16_t gate_level, uint16_t tilt_deg,
                          float dt_scale,
                          float* prev_x, float* prev_y,
                          float* wob_p, float* wob_v, float* bias, float* gate, float* sig) {

    // ---- STEP 1: Cartesian smoothing & Analog Polar Angle-Lock ----
    float target_x = (float)raw_x;
    float target_y = (float)raw_y;

    float raw_mag = sqrtf(target_x * target_x + target_y * target_y);
    float prev_mag = sqrtf(*prev_x * *prev_x + *prev_y * *prev_y);

    // If magnitude is dropping even slightly (slow analog release), lock the angle!
    // This irons out the jagged staircase caused by staggered analog switch fading.
    if (raw_mag < prev_mag - 25.0f) {
        float raw_angle = (raw_mag > 0.1f) ? atan2f(target_y, target_x) : 0.0f;
        float prev_angle = (prev_mag > 0.1f) ? atan2f(*prev_y, *prev_x) : 0.0f;

        float angle_diff = raw_angle - prev_angle;
        // Normalize angle to find the shortest path
        while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
        while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;

        // Apply heavy damping to the angle ONLY. 
        // Magnitude drops raw, ensuring zero input lag on release speed.
        float angle_alpha = 0.10f * dt_scale;
        if (angle_alpha > 1.0f) angle_alpha = 1.0f;
        
        float smoothed_angle = prev_angle + angle_alpha * angle_diff;
        
        // Reconstruct target Cartesian coordinates
        target_x = raw_mag * cosf(smoothed_angle);
        target_y = raw_mag * sinf(smoothed_angle);
    }

    // Standard Cartesian smoothing for base movement
    float alpha = 1.0f;
    if (smoothing_rate > 0) {
        float base = 1.0f - (smoothing_rate / 100.0f * 0.98f);
        alpha = 1.0f - powf(1.0f - base, dt_scale);
    }

    float sm_x = *prev_x + alpha * (target_x - *prev_x);
    float sm_y = *prev_y + alpha * (target_y - *prev_y);
    *prev_x = sm_x;
    *prev_y = sm_y;

    float mag = sqrtf(sm_x * sm_x + sm_y * sm_y);
    float angle = (mag > 0.0001f) ? atan2f(sm_y, sm_x) : 0.0f;

    float center_fade = mag / 2000.0f;
    if (center_fade > 1.0f) center_fade = 1.0f;

    float deflection = mag / AXIS_MAX;
    if (deflection > 1.0f) deflection = 1.0f;

    // ---- STEP 2: THE UNIFIED PHYSICS ENGINE (Always Runs) ----
    {
        float theta_sig = 0.0008f * dt_scale;
        *sig += theta_sig * (1.0f - *sig) + 0.02f * dt_scale * gauss(h);
        if (*sig < 0.5f) *sig = 0.5f;
        if (*sig > 1.6f) *sig = 1.6f;
    }

    float stiffness = 0.01f;  
    float damping = 0.05f;    
    float noise_force = 0.006f * gauss(h) * (*sig); 

    *wob_v += (-stiffness * (*wob_p) - damping * (*wob_v) + noise_force) * dt_scale;
    *wob_p += (*wob_v) * dt_scale;

    if (*wob_p >  1.2f) { *wob_p =  1.2f; *wob_v *= -0.5f; }
    if (*wob_p < -1.2f) { *wob_p = -1.2f; *wob_v *= -0.5f; }

    // ---- STEP 3: Apply Axis Deviation (Wobble) ----
    if (deviation_level > 0) {
        float max_wobble = (deviation_level / 100.0f) * (20.0f * (M_PI / 180.0f));
        float curve = 0.15f + (0.85f * deflection);
        angle += (*wob_p) * max_wobble * curve * center_fade;
    }

    // ---- STEP 4: Ergonomic Tilt (Wandering Baseline) ----
    if (tilt_deg > 0) {
        float center_rad = -((float)tilt_deg) * (M_PI / 180.0f);
        float max_wander = 0.30f * fabsf(center_rad);
        float target_bias = center_rad + (*wob_p * max_wander);
        *bias += 0.015f * dt_scale * (target_bias - *bias);
        angle += (*bias) * center_fade;
    } else {
        *bias += 0.02f * dt_scale * (0.0f - *bias);
    }

    // ---- STEP 5: Gate Slop (Outer Ring Wander) ----
    {
        float target_gate = (*wob_v) * 10.0f; 
        if (target_gate > 1.0f) target_gate = 1.0f;
        if (target_gate < -1.0f) target_gate = -1.0f;
        *gate += 0.02f * dt_scale * (target_gate - *gate);
    }
    float gate_amt = (gate_level / 100.0f) * 0.03f;
    float dynamic_max_gate = AXIS_MAX * (1.0f - gate_amt * (0.5f + 0.5f * (*gate)));

    // ---- STEP 6: Circularity Error ----
    float limit = dynamic_max_gate * (1.0f + (error_pct / 100.0f));
    if (mag > limit) mag = limit;

    // ---- STEP 7: Back to Cartesian ----
    float final_x = mag * cosf(angle);
    float final_y = mag * sinf(angle);
    if (final_x >  AXIS_MAX) final_x =  AXIS_MAX;
    if (final_x < -AXIS_MAX) final_x = -AXIS_MAX;
    if (final_y >  AXIS_MAX) final_y =  AXIS_MAX;
    if (final_y < -AXIS_MAX) final_y = -AXIS_MAX;

    *out_x = (int16_t)final_x;
    *out_y = (int16_t)final_y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry,
                       uint16_t circ_error, uint16_t axis_deviation,
                       uint16_t smoothing_rate, uint16_t gate_level,
                       uint16_t tilt_deg, uint16_t passthrough) {

    if (passthrough) return;

    uint64_t now = time_us_64();
    float float_dt = 1.0f;
    if (h->have_last) {
        float dt = (float)(now - h->last_us) * 1e-6f;
        if (dt < 0.0f) dt = 0.0f;
        float_dt = dt * REF_HZ;
        if (float_dt > 4.0f) float_dt = 4.0f;
        if (float_dt < 0.05f) float_dt = 0.05f;
    }
    h->last_us = now;
    h->have_last = 1;

    // LEFT STICK ONLY
    process_stick(h, lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, gate_level, tilt_deg,
                  float_dt, &h->prev_x_l, &h->prev_y_l, &h->wob_p_l, &h->wob_v_l, &h->bias_l, &h->gate_l, &h->sig_l);

    // RIGHT STICK PASSTHROUGH
    (void)rx;
    (void)ry;
}
