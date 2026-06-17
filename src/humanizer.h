#ifndef HUMANIZER_H
#define HUMANIZER_H

#include <stdint.h>

// We will add memory variables here in later phases!
typedef struct {
    int dummy_init; // Keeps strict C compilers happy for now
} Humanizer;

void humanizer_init(Humanizer* h);

void humanizer_process(Humanizer* h, int16_t* lx, int16_t* ly, int16_t* rx, int16_t* ry, 
                       uint16_t circ_error);

#endif
