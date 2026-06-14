#ifndef _HUMANIZER_H_
#define _HUMANIZER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // Magnitude cap (Q16.16 fixed point, 0x0000D999 = 0.85)
    int32_t magnitude_cap;
    // Drift strength (Q16.16, 0x00001470 = 0.08)
    int32_t drift_strength;
    // Maximum drift (Q16.16, 0x00004000 = 0.25)
    int32_t drift_max;
    // How often drift target changes (in frames)
    uint32_t drift_retarget_frames;
    // Idle threshold (Q16.16, 0x00001999 = 0.10)
    int32_t idle_threshold;
    // Enable/disable
    bool enabled;
} HumanizerSettings;

typedef struct {
    HumanizerSettings settings;

    int32_t drift_lx, drift_ly;
    int32_t drift_rx, drift_ry;
    int32_t target_lx, target_ly;
    int32_t target_rx, target_ry;
    uint32_t retarget_counter_l;
    uint32_t retarget_counter_r;
    bool was_idle_l;
    bool was_idle_r;
    uint32_t rng_state;
} Humanizer;

void humanizer_init(Humanizer* h);
void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry);

#endif
