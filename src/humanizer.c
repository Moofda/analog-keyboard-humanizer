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

    // ---- STEP 1: Cartesian smoothing (straight lines, no slingshot) ----
    float alpha = 1.0f;
    if (smoothing_rate > 0) {
        float base = 1.0f - (smoothing_rate / 100.0f * 0.98f);
        alpha = 1.0f - powf(1.0f - base, dt_scale);
    }
    float sm_x = *prev_x + alpha * ((float)raw_x - *prev_x);
    float sm_y = *prev_y + alpha * ((float)raw_y - *prev_y);
    *prev_x = sm_x;
    *prev_y = sm_y;

    float mag = sqrtf(sm_x * sm_x + sm_y * sm_y);
    float angle = (mag > 0.0001f) ? atan2f(sm_y, sm_x) : 0.0f;

    float center_fade = mag / 2000.0f;
    if (center_fade > 1.0f) center_fade = 1.0f;

    float deflection = mag / AXIS_MAX;
    if (deflection > 1.0f) deflection = 1.0f;

    // ---- non-stationary sigma: slowly wander noise intensity ----
    {
        float theta_sig = 0.0008f * dt_scale;
        *sig += theta_sig * (1.0f - *sig) + 0.02f * dt_scale * gauss(h);
        if (*sig < 0.5f) *sig = 0.5f;
        if (*sig > 1.6f) *sig = 1.6f;
    }

    // ---- STEP 2: The "Organic Wave" (Stochastic Damped Oscillator) ----
    if (deviation_level > 0) {
        float max_wobble = (deviation_level / 100.0f) * (20.0f * (M_PI / 180.0f));
        
        // Mass-Spring-Damper Physics
        float stiffness = 0.01f;  // Determines the frequency/speed of the waves
        float damping = 0.05f;    // Smoothes out the jitter into rolling curves
        float noise_force = 0.006f * gauss(h) * (*sig); // The random wind pushing the thumb

        // Update velocity, then update position
        *wob_v += (-stiffness * (*wob_p) - damping * (*wob_v) + noise_force) * dt_scale;
        *wob_p += (*wob_v) * dt_scale;

        // Soft clamp to prevent physics explosion from edge cases
        if (*wob_p >  1.2f) { *wob_p =  1.2f; *wob_v *= -0.5f; }
        if (*wob_p < -1.2f) { *wob_p = -1.2f; *wob_v *= -0.5f; }

        // INVERTED curve: precise near center, gracefully sloppy at full deflection.
        float curve = 0.15f + (0.85f * deflection);
        angle += (*wob_p) * max_wobble * curve * center_fade;
    }

    // ---- wandering ergonomic tilt ----
    if (tilt_deg > 0) {
        float center_rad = -((float)tilt_deg) * (M_PI / 180.0f);
        float wander = 0.30f * fabsf(center_rad);
        float theta_b = 0.0006f * dt_scale;
        *bias += theta_b * (center_rad - *bias) + wander * dt_scale * gauss(h);
        float lo = center_rad - wander, hi = center_rad + wander;
        if (*bias < lo) *bias = lo;
        if (*bias > hi) *bias = hi;
        angle += (*bias) * center_fade;
    } else {
        *bias += 0.01f * dt_scale * (0.0f - *bias);
    }

    // ---- gate as bounded random walk on outer radius (hall-effect rim slop) ----
    {
        // Lowered the sigma slightly from Claude's version to make outer gate visually smoother
        float theta_g = 0.02f * dt_scale;
        float sigma_g = 0.008f * dt_scale * (*sig); 
        *gate += -theta_g * (*gate) + sigma_g * gauss(h);
        if (*gate >  1.0f) *gate =  1.0f;
        if (*gate < -1.0f) *gate = -1.0f;
    }
    float gate_amt = (gate_level / 100.0f) * 0.03f;
    float dynamic_max_gate = AXIS_MAX * (1.0f - gate_amt * (0.5f + 0.5f * (*gate)));

    // ---- circularity error (static shape) ----
    float limit = dynamic_max_gate * (1.0f + (error_pct / 100.0f));
    if (mag > limit) mag = limit;

    // ---- STEP 3: back to cartesian ----
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
    float dt_scale = 1.0f;
    if (h->have_last) {
        float dt = (float)(now - h->last_us) * 1e-6f;
        if (dt < 0.0f) dt = 0.0f;
        dt_scale = dt * REF_HZ;
        if (dt_scale > 4.0f) dt_scale = 4.0f;
        if (dt_scale < 0.05f) dt_scale = 0.05f;
    }
    h->last_us = now;
    h->have_last = 1;

    // LEFT STICK ONLY — full humanizer treatment (movement)
    process_stick(h, lx, ly, *lx, *ly, circ_error, axis_deviation, smoothing_rate, gate_level, tilt_deg,
                  dt_scale, &h->prev_x_l, &h->prev_y_l, &h->wob_p_l, &h->wob_v_l, &h->bias_l, &h->gate_l, &h->sig_l);

    // RIGHT STICK — untouched passthrough (aim must stay pristine)
    (void)rx;
    (void)ry;
}
