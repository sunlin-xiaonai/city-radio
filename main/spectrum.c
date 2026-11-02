#include "spectrum.h"

#include <math.h>
#include <stdint.h>

#include "stations.h"

#define WIDTH 5.5f  // steps: the peak falls to e^-5 of its height this far from its centre

int spec_step(int mhz10) {
    const int i = mhz10 - FM_MIN_10;
    return i < 0 ? 0 : i >= SPEC_STEPS ? SPEC_STEPS - 1 : i;
}

/** Up to a pixel of noise, the same every time for a step. */
static float noise(int i) {
    uint32_t x = (uint32_t)i * 2654435761u;
    x ^= x >> 15;
    return (float)(x & 0xff) / 255.0f;
}

float spec_peak(int mhz10, float height, int i) {
    const float d = (float)(i - spec_step(mhz10)) / WIDTH;
    return height * expf(-5.0f * d * d);
}

float spec_height(int city, int skip, int i) {
    float h = noise(i);
    for (int s = 0; s < city_stations(city); s++)
        if (s != skip) h += spec_peak(station_at(city, s)->mhz10, SPEC_REST, i);
    return h;
}
