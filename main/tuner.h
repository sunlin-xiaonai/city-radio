// The tuner face: the frequency large in the middle and, along the bottom of the round screen, a signal line across
// the FM band (87.5 .. 108 MHz, low on the left) with a peak at each of the city's stations, the one playing coral and
// moving with what the speaker plays. LVGL: call with the board lock held (or from LVGL callbacks).
#pragma once

#include <stdbool.h>

#include "lvgl.h"

void tuner_create(lv_obj_t *screen);
/** The city and the station's name in English (the owner's language) from the next tuner_show_station, else Chinese
 *  (the default). */
void tuner_set_english(bool on);
/** Shows the city's station at this index: its peak turns coral. `slide` (tuning) lets the new peak rise from the
 *  others' height; else it keeps its height. */
void tuner_show_station(int city, int index, bool slide);
/** The playing station's peak: `level` 0..100 from yk_speak_level(); `live` false lays it down to the others'
 *  height (paused, connecting). */
void tuner_level(int level, bool live);
