#include "humanizer.h"
#include "pico/stdlib.h"
#include <stdlib.h>
#include <math.h>

// Fast, low-overhead PRNG to keep execution speeds blazing fast
static uint32_t local_prng(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

void humanizer_init(Humanizer* h) {
    h->rng_state = 0x8A5E4D3C; 
    h->wander_angle_l = 0.0f;
    h->wander_angle_r = 0.0f;
    h->drift_lx = 0;
    h->drift_ly = 0;
    h->drift_rx = 0;
    h->drift_ry = 0;
    h->last_drift_time = 0;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, uint16_t angle_spread, uint16_t deflection_scale, uint16_t deadzone) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // ----------------------------------------------------------------
    // 1. IMPERFECT CENTERING SPRING (DRIFT MODULE)
    // ----------------------------------------------------------------
    if (deadzone > 0) {
        if (now - h->last_drift_time > 200) { // Evolve spring settling point every 200ms
            h->last_drift_time = now;
            int32_t max_drift = deadzone * 10; 
            h->drift_lx = (int16_t)((local_prng(&h->rng_state) % (max_drift * 2)) - max_drift);
            h->drift_ly = (int16_t)((local_prng(&h->rng_state) % (max_drift * 2)) - max_drift);
            h->drift_rx = (int16_t)((local_prng(&h->rng_state) % (max_drift * 2)) - max_drift);
            h->drift_ry = (int16_t)((local_prng(&h->rng_state) % (max_drift * 2)) - max_drift);
        }
        
        if (*lx == 0 && *ly == 0) { *lx = h->drift_lx; *ly = h->drift_ly; }
        if (*rx == 0 && *ry == 0) { *rx = h->drift_rx; *ry = h->drift_ry; }
    }

    // ----------------------------------------------------------------
    // 2. POLAR COORDINATE DYNAMIC WANDERING (LEFT STICK)
    // ----------------------------------------------------------------
    if (angle_spread > 0 && (*lx != 0 || *ly != 0)) {
        // Calculate the vector magnitude (r) and target angle (theta)
        float fx = (float)*lx;
        float fy = (float)*ly;
        float r = sqrtf(fx * fx + fy * fy);
        float theta = atan2f(fy, fx);

        // Map the Slider 1 value (0-100) into an absolute maximum error cone in radians
        // Max value of 100 yields ~0.25 radians (approx ±15 degrees maximum warp area)
        float max_cone = (angle_spread / 100.0f) * 0.25f;

        // Apply Slider 2 (Deflection Scaling): Shrink the error cone as the stick pushes out.
        // Pinned to the outer gate rim = maximum physical control stability, error drops.
        float current_max_cone = max_cone * (1.0f - (r / 32768.0f) * (deflection_scale / 100.0f) * 0.75f);
        if (current_max_cone < 0.0f) current_max_cone = 0.0f;

        // BOUNDED RANDOM WALK (Slow & Smooth Filter):
        // Calculate a micro-fractional push between -0.02 and +0.02 radians
        float step = (((float)(local_prng(&h->rng_state) % 1000) / 1000.0f) * 0.04f) - 0.02f;
        
        // Add step to the persistent memory to create an organic wave over time
        h->wander_angle_l += step;

        // Enforce the boundaries of the dynamically scaled error cone
        if (h->wander_angle_l > current_max_cone)  h->wander_angle_l = current_max_cone;
        if (h->wander_angle_l < -current_max_cone) h->wander_angle_l = -current_max_cone;

        // Translate Polar data smoothly right back into standard Cartesian coordinates
        *lx = (int16_t)(r * cosf(theta + h->wander_angle_l));
        *ly = (int16_t)(r * sinf(theta + h->wander_angle_l));
    } else {
        h->wander_angle_l = 0.0f; // Snaps baseline clean if inputs drop
    }

    // ----------------------------------------------------------------
    // 3. POLAR COORDINATE DYNAMIC WANDERING (RIGHT STICK)
    // ----------------------------------------------------------------
    if (angle_spread > 0 && (*rx != 0 || *ry != 0)) {
        float fx = (float)*rx;
        float fy = (float)*ry;
        float r = sqrtf(fx * fx + fy * fy);
        float theta = atan2f(fy, fx);

        float max_cone = (angle_spread / 100.0f) * 0.25f;
        float current_max_cone = max_cone * (1.0f - (r / 32768.0f) * (deflection_scale / 100.0f) * 0.75f);
        if (current_max_cone < 0.0f) current_max_cone = 0.0f;

        float step = (((float)(local_prng(&h->rng_state) % 1000) / 1000.0f) * 0.04f) - 0.02f;
        h->wander_angle_r += step;

        if (h->wander_angle_r > current_max_cone)  h->wander_angle_r = current_max_cone;
        if (h->wander_angle_r < -current_max_cone) h->wander_angle_r = -current_max_cone;

        *rx = (int16_t)(r * cosf(theta + h->wander_angle_r));
        *ry = (int16_t)(r * sinf(theta + h->wander_angle_r));
    } else {
        h->wander_angle_r = 0.0f;
    }
}
