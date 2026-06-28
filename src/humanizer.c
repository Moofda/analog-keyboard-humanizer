#include "humanizer.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * (float)M_PI)
#endif

// NOTE (test branch): drift texture amount, gate wobble strength, and wobble
// frequency used to be #define constants. They are now LIVE, tunable params
// passed in from the web UI / flash config (texture_walk, texture_sprint,
// wobble_deg, wobble_freq). See process_left_stick() below.

static float clamp_abs(float val, float max_val) {
    if (val > max_val) return max_val;
    if (val < -max_val) return -max_val;
    return val;
}

void humanizer_init(Humanizer* h) {
    h->drift_x = 0.0f; h->drift_y = 0.0f;
    h->target_x = 0.0f; h->target_y = 0.0f;
    h->gate_state = 0.0f;
    h->stride_state = 0.0f;
    h->pos_lx = 0.0f; h->pos_ly = 0.0f;
    h->vel_lx = 0.0f; h->vel_ly = 0.0f;
    h->was_active_l = false; 
    h->land_offset_l = 0.0f;
    // Test-branch additions
    h->osc_phase[0] = 0.0f; h->osc_phase[1] = 1.7f;  // arbitrary phase spread
    h->osc_phase[2] = 0.0f; h->osc_phase[3] = 4.1f;
    h->wobble_phase = 0.0f;
}

static void process_left_stick(Humanizer* h, int16_t* axis_x, int16_t* axis_y, 
                          uint16_t circ_error, uint16_t smoothing_rate, uint16_t anti_deadzone, 
                          uint16_t walk_drift, uint16_t sprint_drift, 
                          uint16_t gate_slip, uint16_t landing_var,
                          uint16_t texture_walk, uint16_t texture_sprint,
                          uint16_t wobble_deg, uint16_t wobble_freq) {
    
    // Normalize Input
    float tx = (float)(*axis_x) / 32767.0f;
    float ty = (float)(*axis_y) / 32767.0f;

    // --- CONTINUOUS BACKGROUND ENTROPY ---
    // 1. Oscillator drift (PRIMARY): a Lissajous-style sway built from sine
    //    waves at slightly different speeds. This is the wave-like body of the
    //    motion -- what a real thumb does -- rather than a random crawl.
    //    Phases advance every tick (dt = 0.004s @ 250Hz). The frequencies are
    //    deliberately irrational ratios so the pattern never exactly repeats.
    h->osc_phase[0] += TWO_PI * 0.31f * 0.004f;
    h->osc_phase[1] += TWO_PI * 0.47f * 0.004f;
    h->osc_phase[2] += TWO_PI * 0.29f * 0.004f;
    h->osc_phase[3] += TWO_PI * 0.53f * 0.004f;
    if (h->osc_phase[0] > TWO_PI) h->osc_phase[0] -= TWO_PI;
    if (h->osc_phase[1] > TWO_PI) h->osc_phase[1] -= TWO_PI;
    if (h->osc_phase[2] > TWO_PI) h->osc_phase[2] -= TWO_PI;
    if (h->osc_phase[3] > TWO_PI) h->osc_phase[3] -= TWO_PI;

    float osc_x = 0.6f * sinf(h->osc_phase[0]) + 0.4f * sinf(h->osc_phase[1]);
    float osc_y = 0.6f * sinf(h->osc_phase[2]) + 0.4f * sinf(h->osc_phase[3]);

    // 1b. Random-crawl TEXTURE (SECONDARY): your original target-seeking drift,
    //     kept as a small layer to break up the oscillator's regularity. The
    //     amount it contributes is now a live, deflection-interpolated param
    //     (texture_walk -> texture_sprint), blended in further below where
    //     deflection is known.
    if (fabsf(h->drift_x - h->target_x) < 0.05f) h->target_x = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    if (fabsf(h->drift_y - h->target_y) < 0.05f) h->target_y = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    h->drift_x += (h->target_x - h->drift_x) * 0.002f;
    h->drift_y += (h->target_y - h->drift_y) * 0.002f;

    // 2. Outer Gate Friction
    float noise_gate = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    h->gate_state = clamp_abs((h->gate_state * 0.80f) + (noise_gate * 0.20f), 1.0f);

    // 1. Anti-Deadzone
    float raw_mag_initial = sqrtf(tx*tx + ty*ty);
    if (anti_deadzone > 0 && raw_mag_initial > 0.01f) {
        float ad_floor = anti_deadzone / 100.0f;
        float scaled_mag = ad_floor + raw_mag_initial * (1.0f - ad_floor);
        tx = (tx / raw_mag_initial) * scaled_mag;
        ty = (ty / raw_mag_initial) * scaled_mag;
    }

    // 2. Circularity
    if (circ_error < 50) {
        float circle_tx = tx * sqrtf(1.0f - (ty * ty) / 2.0f);
        float circle_ty = ty * sqrtf(1.0f - (tx * tx) / 2.0f);
        float blend = circ_error / 50.0f;
        tx = circle_tx + (tx - circle_tx) * blend;
        ty = circle_ty + (ty - circle_ty) * blend;
    }

    // --- CALCULATE TARGET COORDINATE ---
    float target_mag = sqrtf(tx*tx + ty*ty);
    float target_angle = atan2f(ty, tx);

    if (target_mag > 0.01f) {
        
        float deflection = target_mag > 1.0f ? 1.0f : target_mag; 

        // A. Initial Stride Offset (The Sweeping Arc)
        if (!(h->was_active_l)) { 
            h->land_offset_l = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f; 
            h->stride_state = 0.0f; // Reset the pivot state
            h->was_active_l = true;
        }

        // The EMA smoothly rolls the stride state toward the target offset
        h->stride_state = (h->stride_state * 0.90f) + (h->land_offset_l * 0.10f);

        float safe_land_deg = (landing_var > 10) ? (landing_var / 100.0f) * 6.0f : (float)landing_var;
        float stride_max = safe_land_deg * (M_PI / 180.0f);
        
        // The angle perfectly scales with physical switch deflection 
        target_angle += (h->stride_state) * stride_max * deflection; 

        // B. Gate Slip 
        if (gate_slip > 0 && target_mag > 0.99f) { 
            float slip_amount = (gate_slip / 100.0f) * 0.03f * fabsf(h->gate_state);
            target_mag -= slip_amount;
        }
        
        // Convert the angle-flawed target back to X/Y space
        tx = cosf(target_angle) * target_mag;
        ty = sinf(target_angle) * target_mag;

        // C. Dynamic Cartesian Drift (oscillator wave + small random texture)
        if (walk_drift > 0 || sprint_drift > 0) {
            float current_drift_deg = (float)walk_drift + ((float)sprint_drift - (float)walk_drift) * deflection;
            float drift_scale = current_drift_deg * (M_PI / 180.0f);

            // Texture amount interpolates walk->sprint by deflection, just like
            // the drift magnitude does. Param is 0-100 in the UI -> 0.0-1.0 here.
            float texture_amt = ((float)texture_walk
                + ((float)texture_sprint - (float)texture_walk) * deflection) / 100.0f;

            // Blend: oscillator wave is the body, random crawl is the seasoning.
            float blended_drift_x = osc_x + h->drift_x * texture_amt;
            float blended_drift_y = osc_y + h->drift_y * texture_amt;

            // Multiplied by deflection to prevent center snap / wiper effect
            tx += blended_drift_x * (drift_scale * deflection);
            ty += blended_drift_y * (drift_scale * deflection);
        }

        // C2. Gate wobble: a tiny angular tremor that only appears near full
        //     deflection, simulating a thumb micro-trembling against the gate.
        //     Fades in between 0.85 and 1.0 deflection so it never affects walks.
        //     Strength (wobble_deg, in tenths of a degree: UI 0-30 -> 0.0-3.0 deg)
        //     and speed (wobble_freq, in Hz: UI 0-20) are both live-tunable.
        if (wobble_deg > 0 && deflection > 0.85f) {
            float wob_hz = (float)wobble_freq;
            if (wob_hz < 0.1f) wob_hz = 0.1f; // avoid a frozen tremor at freq 0
            h->wobble_phase += TWO_PI * wob_hz * 0.004f;
            if (h->wobble_phase > TWO_PI) h->wobble_phase -= TWO_PI;
            float wobble_fade = (deflection - 0.85f) / 0.15f; // 0..1
            float wob_max_deg = (float)wobble_deg / 10.0f;     // 0-30 -> 0.0-3.0 deg
            float wobble_rad = sinf(h->wobble_phase) * wob_max_deg * (M_PI / 180.0f) * wobble_fade;
            float cw = cosf(wobble_rad), sw = sinf(wobble_rad);
            float rx_ = tx * cw - ty * sw;
            float ry_ = tx * sw + ty * cw;
            tx = rx_; ty = ry_;
        }

        // Final boundary clamp to prevent pushes past square bounds.
        // NOTE: this scales tx and ty by the SAME ratio, so it only corrects
        // magnitude -- your stride lean (the off-axis angle) is preserved.
        // That's why pegging "right" keeps its small imperfect lean here.
        float final_target_angle = atan2f(ty, tx);
        float square_max = 1.0f / fmaxf(fabsf(cosf(final_target_angle)), fabsf(sinf(final_target_angle)));
        float allowed_max = 1.0f + (square_max - 1.0f) * (circ_error / 50.0f);
        float final_target_mag = sqrtf(tx*tx + ty*ty);
        if (final_target_mag > allowed_max) {
            tx = (tx / final_target_mag) * allowed_max;
            ty = (ty / final_target_mag) * allowed_max;
        }

    } else {
        h->was_active_l = false;
        tx = 0.0f; ty = 0.0f;
    }

    // --- THE PHYSICAL FILTER ---
    // 4. Inertia Smoothing (The heavy physical spring)
    if (smoothing_rate == 0) {
        h->pos_lx = tx; h->pos_ly = ty;
        h->vel_lx = 0.0f; h->vel_ly = 0.0f;
    } else {
        float freq_hz = 25.0f - (smoothing_rate * 0.22f); 
        if (freq_hz < 3.0f) freq_hz = 3.0f; 
        float w = TWO_PI * freq_hz;
        float k = w * w;      
        float c = 2.0f * w;   
        float dt = 0.004f;    
        
        float force_x = k * (tx - h->pos_lx) - c * (h->vel_lx);
        float force_y = k * (ty - h->pos_ly) - c * (h->vel_ly);
        
        h->vel_lx += force_x * dt; h->vel_ly += force_y * dt;
        h->pos_lx += (h->vel_lx) * dt; h->pos_ly += (h->vel_ly) * dt;
    }
    
    // 5. Magnitude Recovery
    float spring_mag = sqrtf((h->pos_lx)*(h->pos_lx) + (h->pos_ly)*(h->pos_ly));
    float final_mag_check = sqrtf(tx*tx + ty*ty);
    if (final_mag_check > 0.95f && spring_mag < 0.95f && spring_mag > 0.1f) {
        float correction = 1.0f + (0.95f - spring_mag) * 0.3f; 
        h->pos_lx *= correction;
        h->pos_ly *= correction;
    }

    float final_x = clamp_abs(h->pos_lx, 1.0f);
    float final_y = clamp_abs(h->pos_ly, 1.0f);

    *axis_x = (int16_t)(final_x * 32767.0f);
    *axis_y = (int16_t)(final_y * 32767.0f);
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry,
                       uint16_t circ_error, uint16_t smoothing_rate, uint16_t anti_deadzone, 
                       uint16_t walk_drift, uint16_t sprint_drift, 
                       uint16_t gate_slip, uint16_t landing_var, uint16_t passthrough,
                       uint16_t texture_walk, uint16_t texture_sprint,
                       uint16_t wobble_deg, uint16_t wobble_freq) {
    
    if (passthrough) return; 

    process_left_stick(h, lx, ly, 
                       circ_error, smoothing_rate, anti_deadzone, 
                       walk_drift, sprint_drift, gate_slip, landing_var,
                       texture_walk, texture_sprint, wobble_deg, wobble_freq);
}
