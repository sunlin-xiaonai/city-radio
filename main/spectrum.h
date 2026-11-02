// The dial's signal line: the FM band from 87.5 to 108 MHz in 0.1 MHz steps, quiet but for a peak at each of the
// city's stations. Heights in pixels above the baseline. Plain C, no ESP-IDF: host tested (test/run.sh).
#pragma once

#define SPEC_STEPS 206   // 87.5 .. 108.0 MHz
#define SPEC_HALF 6      // a peak's half width in steps: beyond it, under a pixel is left
#define SPEC_REST 22.0f  // a station's peak when it is not the one playing (or nothing plays)
#define SPEC_LIVE 46.0f  // the playing station's peak at full level

/** The step of a frequency (0.1 MHz), clamped into the band. */
int spec_step(int mhz10);
/** The line at step i: a little static noise and the peaks of the city's stations at SPEC_REST, but for the
 *  station at index `skip` (-1: none). */
float spec_height(int city, int skip, int i);
/** One peak of this height centred on a frequency, at step i. */
float spec_peak(int mhz10, float height, int i);
