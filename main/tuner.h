// The tuner face: an FM dial along the bottom of the round screen (87.5 .. 108 MHz, low on the left) with a dot per
// station of the city and a coral needle that slides to the station tuned, the frequency large in the middle, and a
// row of level bars that move with what the speaker plays. LVGL: call with the board lock held (or from LVGL callbacks).
#pragma once

#include <stdbool.h>

#include "lvgl.h"

void tuner_create(lv_obj_t *screen);
/** The city and the station's name in English (the owner's language) from the next tuner_show_station, else Chinese
 *  (the default). */
void tuner_set_english(bool on);
/** Shows the city's station at this index, with a dot on the dial per station of the city; `slide` moves the needle
 *  there (else it jumps). */
void tuner_show_station(int city, int index, bool slide);
/** Level bars: `level` 0..100 from yk_speak_level(); `live` false lays them flat and grey (paused, connecting). */
void tuner_level(int level, bool live);
