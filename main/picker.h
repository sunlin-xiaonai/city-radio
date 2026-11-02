// The city picker, over the tuner: the cities on a wheel in the middle of the round screen, the one in the middle on
// a coral bar, fading out towards the top and the bottom of the circle. Drag to turn it; tap a city to choose it.
// The side key's tap goes back without changing (round-screen-design.md 6: a tap goes back one level). LVGL: call
// with the board lock held (or from LVGL callbacks).
#pragma once

#include <stdbool.h>

#include "lvgl.h"

void picker_create(lv_obj_t *screen);
/** Opens it on this city, in the screen's language. */
void picker_open(int city, bool english);
void picker_close(void);
bool picker_is_open(void);
/** The city tapped since it opened (and clears it), or -1. */
int picker_take_choice(void);
