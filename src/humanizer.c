#include "humanizer.h"
#include <math.h>

// The absolute maximum value XInput will accept
#define AXIS_MAX 32767.0f 

void humanizer_init(Humanizer* h) {
    h->dummy_init = 1;
}

// Internal helper to apply the circular gate to a single stick
void process_stick(int16_t* out_x, int16_t* out_y, int16_t raw_x, int16_t raw_y, uint16_t error_pct) {
    float x = (float)raw_x;
    float y = (float)raw_y;

    // Calculate how far the stick was pushed (Magnitude)
    float magnitude = sqrtf((x * x) + (y * y));
    
    if (magnitude > 0) {
        // Calculate the physical size of our "gate"
        // 0% = Perfect Circle (32767 max radius)
        // 3% = Slightly stretched circle (33750 max radius)
        float limit = AXIS_MAX * (1.0f + (error_pct / 100.0f));

        // If the keyboard pushes into the square corners past our gate limit, clamp it!
        if (magnitude > limit) {
            float angle = atan2f(y, x);
            x = limit * cosf(angle);
            y = limit * sinf(angle);
        }
    }

    // Strict hardware clamping to prevent XInput from crashing if we overshoot
    if (x > AXIS_MAX) x = AXIS_MAX;
    if (x < -AXIS_MAX) x = -AXIS_MAX;
    if (y > AXIS_MAX) y = AXIS_MAX;
    if (y < -AXIS_MAX) y = -AXIS_MAX;

    // Output back to the controller pipeline
    *out_x = (int16_t)x;
    *out_y = (int16_t)y;
}

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error) {
    
    // Process Left Stick
    process_stick(lx, ly, *lx, *ly, circ_error);
    
    // Process Right Stick
    process_stick(rx, ry, *rx, *ry, circ_error);
}
