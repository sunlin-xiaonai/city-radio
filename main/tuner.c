#include "tuner.h"

#include <math.h>
#include <stdio.h>

#include "spectrum.h"
#include "stations.h"
#include "yaykit_ui.h"

// The city along the top of the circle, the frequency large in the middle, and along the bottom a quiet signal line
// across the FM band with a peak at each of the city's stations: sage, the one playing coral, rising and falling with
// what the speaker plays. All inside the radius-200 content circle. LVGL measures angles clockwise from 3 o'clock;
// the line is its bottom quarter, 45 .. 135 degrees, low frequencies on the left. One function (point_at) places
// every point, so a peak always sits where its frequency is.
#define CITY_R 178   // the city's baseline along the top
#define LINE_R 192   // the signal line's baseline; peaks rise inwards from it
#define LINE_FROM 45
#define LINE_SPAN 90
#define ENDS_R 186   // 88 and 108, just past the line's ends
#define ENDS_DEG 6

#define FM_Y 102
#define FREQ_Y 118
#define NAME_Y 250

#define WINDOW (2 * SPEC_HALF + 1)

static lv_obj_t *s_screen, *s_city, *s_freq, *s_name;
static lv_obj_t *s_left, *s_right;              // the quiet line either side of the playing station's peak
static lv_obj_t *s_peaks[CITY_MAX_STATIONS];    // the other stations' peaks, over the quiet line
static lv_obj_t *s_live;                        // the playing station's peak
static lv_point_precise_t s_line[SPEC_STEPS];   // the line without the playing station's peak
static lv_point_precise_t s_live_pts[WINDOW];
static int s_city_shown = -1, s_station_shown = -1, s_live_from, s_live_n;
static float s_live_h = SPEC_REST, s_live_drawn = -1;
static bool s_english;

/** Step i of the band at h pixels above the baseline. */
static void point_at(int i, float h, lv_point_precise_t *p) {
    const float deg = LINE_FROM + LINE_SPAN * (float)(SPEC_STEPS - 1 - i) / (SPEC_STEPS - 1);
    const float a = deg * (float)M_PI / 180.0f, r = LINE_R - h;
    p->x = YK_CX + r * cosf(a);
    p->y = YK_CY + r * sinf(a);
}

static lv_obj_t *line(lv_obj_t *parent, uint32_t color, int width, lv_opa_t opa) {
    lv_obj_t *o = lv_line_create(parent);
    lv_obj_set_style_line_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(o, width, 0);
    lv_obj_set_style_line_opa(o, opa, 0);
    lv_obj_set_style_line_rounded(o, true, 0);
    lv_obj_set_clickable(o, false);
    return o;
}

/** A small label centred on a point. */
static void label_at(lv_obj_t *parent, const char *text, float deg, int r) {
    const float a = deg * (float)M_PI / 180.0f;
    const int x = (int)lroundf(YK_CX + r * cosf(a)), y = (int)lroundf(YK_CY + r * sinf(a));
    yk_ui_label(parent, text, x - 30, y - 11, 60, YK_SIZE_MIN, YK_INK3);
}

void tuner_set_english(bool on) { s_english = on; }

void tuner_create(lv_obj_t *screen) {
    s_screen = screen;
    s_left = line(screen, YK_INK3, 2, LV_OPA_70);
    s_right = line(screen, YK_INK3, 2, LV_OPA_70);
    for (int i = 0; i < CITY_MAX_STATIONS; i++) s_peaks[i] = line(screen, YK_SAGE, 3, LV_OPA_COVER);
    s_live = line(screen, YK_CORAL, 4, LV_OPA_COVER);
    label_at(screen, "88", LINE_FROM + LINE_SPAN + ENDS_DEG, ENDS_R);
    label_at(screen, "108", LINE_FROM - ENDS_DEG, ENDS_R);

    yk_ui_label(screen, "FM", 0, FM_Y, 466, YK_SIZE_MIN, YK_INK3);
    s_freq = yk_ui_label(screen, "", 0, FREQ_Y, 466, YK_SIZE_HERO_MIN, YK_INK);
    s_name = yk_ui_label(screen, "", 0, NAME_Y, 466, YK_SIZE_HEADING, YK_INK2);
}

/** The steps of a peak's window, clamped into the band. */
static void window(int mhz10, int *from, int *n) {
    const int c = spec_step(mhz10);
    const int lo = c - SPEC_HALF < 0 ? 0 : c - SPEC_HALF;
    const int hi = c + SPEC_HALF >= SPEC_STEPS ? SPEC_STEPS - 1 : c + SPEC_HALF;
    *from = lo, *n = hi - lo + 1;
}

/** The playing station's peak at its height now (the quiet line under it, plus the peak). */
static void draw_live(void) {
    const int mhz10 = station_at(s_city_shown, s_station_shown)->mhz10;
    for (int k = 0; k < s_live_n; k++) {
        const int i = s_live_from + k;
        point_at(i, spec_height(s_city_shown, s_station_shown, i) + spec_peak(mhz10, s_live_h, i), &s_live_pts[k]);
    }
    lv_line_set_points(s_live, s_live_pts, s_live_n);
    s_live_drawn = s_live_h;
}

/** The quiet line and the other stations' peaks, for the station playing. */
static void draw_line(int city, int index) {
    s_city_shown = city, s_station_shown = index;
    for (int i = 0; i < SPEC_STEPS; i++) point_at(i, spec_height(city, index, i), &s_line[i]);
    window(station_at(city, index)->mhz10, &s_live_from, &s_live_n);
    // The quiet line stops where the playing station's peak starts and goes on where it ends.
    const int end = s_live_from + s_live_n - 1;
    lv_line_set_points(s_left, s_line, s_live_from + 1);
    lv_line_set_points(s_right, &s_line[end], SPEC_STEPS - end);
    for (int s = 0; s < CITY_MAX_STATIONS; s++) {
        const bool other = s < city_stations(city) && s != index;
        lv_obj_set_hidden(s_peaks[s], !other);
        if (!other) continue;
        int from, n;
        window(station_at(city, s)->mhz10, &from, &n);
        lv_line_set_points(s_peaks[s], &s_line[from], n);
    }
    draw_live();
}

void tuner_show_station(int city, int index, bool slide) {
    const station_t *st = station_at(city, index);
    char text[16];
    station_freq_text(st, text, sizeof(text));
    lv_label_set_text(s_freq, text);
    lv_label_set_text(s_name, station_name(st, s_english));
    // The city along the top of the circle (one label per character: short names only).
    if (s_city) lv_obj_delete(s_city);
    s_city = yk_ui_arc_label(s_screen, station_city(st, s_english), YK_SIZE_STATUS, YK_SAGE_TEXT, 0, CITY_R, false);
    index = station_step(city, index, 0);
    if (city == s_city_shown && index == s_station_shown) return;
    if (slide) s_live_h = SPEC_REST;  // the new station's peak rises from where the others are
    draw_line(city, index);
}

void tuner_level(int level, bool live) {
    static uint32_t seed = 0x2545F491;
    if (s_station_shown < 0) return;
    float target = SPEC_REST;
    if (live) {
        // With the speaker's level, a little unsteady like a real signal.
        seed = seed * 1664525u + 1013904223u;
        const float wobble = 0.8f + 0.2f * (float)(seed >> 24) / 255.0f;
        target = SPEC_REST + (SPEC_LIVE - SPEC_REST) * (level / 100.0f) * wobble;
    }
    // Up fast, down slowly.
    s_live_h += (target - s_live_h) * (target > s_live_h ? 0.6f : 0.2f);
    if (fabsf(s_live_h - s_live_drawn) >= 0.5f) draw_live();
}
