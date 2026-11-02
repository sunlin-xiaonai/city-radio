#include "picker.h"

#include <string.h>

#include "cjk_font.h"
#include "stations.h"
#include "yaykit_ui.h"

// The wheel: five rows of 52 px (a 24 px name and 28 px between), about 260 px tall around the centre. At its top
// and bottom edge (130 px from the centre) the radius-200 content circle is 300 px wide, so a 240 px wheel stays
// inside. The roller draws the coral bar under the middle row across its whole width.
#define ROWS 5
#define ROW_GAP 28
#define WHEEL_W 240
#define FADE_H 78   // the top and bottom rows fade into the background
#define TITLE_R 178 // the title along the top of the circle, where the tuner writes the city
#define HINT_Y 382  // under the wheel's last row; the circle is still 210 px wide at its foot

static lv_obj_t *s_box, *s_wheel, *s_title, *s_hint;
static int s_choice = -1;
static char s_options[256];

static void on_clicked(lv_event_t *e) {
    // A tap: the roller has already turned the tapped city into the middle. (It stops the click that ends a drag.)
    s_choice = (int)lv_roller_get_selected(lv_event_get_target_obj(e));
}

/** A band over the wheel's top or bottom rows, the background colour fading to nothing towards the middle. */
static void fade(int y, bool top) {
    lv_obj_t *o = lv_obj_create(s_box);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, WHEEL_W, FADE_H);
    lv_obj_set_pos(o, YK_CX - WHEEL_W / 2, y);
    lv_obj_set_style_bg_color(o, lv_color_hex(YK_PAPER), 0);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(YK_PAPER), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(o, top ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_grad_opa(o, top ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_clickable(o, false);  // taps reach the wheel underneath
}

/** The city names in the screen's language, one per row. */
static void fill(bool english) {
    s_options[0] = '\0';
    for (int c = 0; c < cities_count(); c++) {
        if (c) strlcat(s_options, "\n", sizeof(s_options));
        strlcat(s_options, city_name(c, english), sizeof(s_options));
    }
    lv_roller_set_options(s_wheel, s_options, LV_ROLLER_MODE_NORMAL);
}

void picker_create(lv_obj_t *screen) {
    s_box = lv_obj_create(screen);
    lv_obj_remove_style_all(s_box);
    lv_obj_set_size(s_box, 466, 466);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(YK_PAPER), 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(s_box, false);

    const lv_font_t *font = cjk_font_get(YK_SIZE_HEADING);
    s_wheel = lv_roller_create(s_box);
    lv_obj_set_width(s_wheel, WHEEL_W);
    if (font) {
        lv_obj_set_style_text_font(s_wheel, font, 0);
        lv_obj_set_style_text_font(s_wheel, font, LV_PART_SELECTED);
    }
    lv_obj_set_style_text_line_space(s_wheel, ROW_GAP, 0);
    lv_obj_set_style_text_align(s_wheel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_wheel, lv_color_hex(YK_INK2), 0);
    lv_obj_set_style_bg_opa(s_wheel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_wheel, 0, 0);
    lv_obj_set_style_pad_all(s_wheel, 0, 0);
    lv_obj_set_style_anim_duration(s_wheel, YK_PAGE_MS, 0);
    // The middle row: dark text on a coral bar.
    lv_obj_set_style_bg_color(s_wheel, lv_color_hex(YK_CORAL), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(s_wheel, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_radius(s_wheel, LV_RADIUS_CIRCLE, LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_wheel, lv_color_hex(YK_PAPER), LV_PART_SELECTED);
    lv_obj_add_event_cb(s_wheel, on_clicked, LV_EVENT_CLICKED, NULL);
    fill(false);
    lv_roller_set_visible_row_count(s_wheel, ROWS);
    lv_obj_center(s_wheel);
    lv_obj_update_layout(s_wheel);
    const int top = lv_obj_get_y(s_wheel), h = lv_obj_get_height(s_wheel);
    fade(top, true);
    fade(top + h - FADE_H, false);

    s_hint = yk_ui_label(s_box, "", 0, HINT_Y, 466, YK_SIZE_MIN, YK_INK3);
    lv_obj_set_hidden(s_box, true);
}

void picker_open(int city, bool english) {
    fill(english);
    lv_roller_set_selected(s_wheel, (uint32_t)city_wrap(city), LV_ANIM_OFF);
    if (s_title) lv_obj_delete(s_title);
    s_title = yk_ui_arc_label(s_box, english ? "Cities" : "选择城市", YK_SIZE_STATUS, YK_SAGE_TEXT, 0, TITLE_R, false);
    lv_label_set_text(s_hint, english ? "Tap a city · Press: back" : "轻点选城市 · 按侧键返回");
    s_choice = -1;
    lv_obj_set_hidden(s_box, false);
    lv_obj_move_foreground(s_box);
}

void picker_close(void) { lv_obj_set_hidden(s_box, true); }

bool picker_is_open(void) { return s_box && !lv_obj_is_hidden(s_box); }

int picker_take_choice(void) {
    const int c = s_choice;
    s_choice = -1;
    return c;
}
