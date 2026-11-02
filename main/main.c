// City Radio: live city stations on the round screen. The start-up order is the template's (firmware guide 6):
// NVS (never erased) -> yaykit_app (crash loop guard) -> board -> speaker -> side key -> Yaykit Link -> screen ->
// console -> yk_app_ready() -> Wi-Fi. A tap on the side key plays or pauses, a swipe left or right changes the
// station, a swipe up opens the city picker, a double click opens the app ring (yaykit_app). The side key's hold is
// not the app's (round-screen-design.md 6). Leaving, it tells Yay what was listened to and for how long.
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "cjk_font.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "picker.h"
#include "radio.h"
#include "stations.h"
#include "tuner.h"
#include "yaykit_account.h"
#include "yaykit_app.h"
#include "yaykit_key.h"
#include "yaykit_link.h"
#include "yaykit_speak.h"
#include "yaykit_ui.h"
#include "yaykit_wifi.h"
#include "yk_hint.h"

#define APP_SLUG "city-radio"
#define NVS_NS "yk_cityradio"
#define HINT_US (20 * 1000000LL)  // how to use it, on the status line: 10 s tuning, then 10 s cities
static const char *TAG = "city-radio";

static int s_city, s_station;  // the station is the city's (stations.h)
static bool s_paused;
static int64_t s_listened_us, s_last_tick_us;  // time actually playing, this visit
static lv_obj_t *s_status;
static int64_t s_hint_until_us;  // the status line says how to use it for the first seconds of playing
static yk_rim_t *s_rim;
static volatile bool s_tapped;          // set in the key task, acted on in the LVGL task
static volatile int s_swipe;            // -1 / +1 from a gesture, acted on in the next tick
static volatile bool s_swiped_up;       // a swipe up: the city picker, in the next tick
static bool s_english;                  // the owner's language is not Chinese: the screen in English

/** The screen's text in its language. */
static const char *T(const char *zh, const char *en) { return s_english ? en : zh; }

/** Through yaykit_app (yk_app_on_key): returns whether the event was used. Double clicks are the system's. */
static bool on_key(const yk_key_event_t *ev, void *ctx) {
    (void)ctx;
    if (ev->type != YK_KEY_TAP) return false;  // a hold is the system's: it offers to go back to Yay and talk
    s_tapped = true;
    return true;
}

/** The needle follows the finger: stations run low to high left to right on the dial, so a swipe to the right tunes
 *  up (the needle moves right) and a swipe to the left tunes down. A swipe up opens the city picker (a swipe down is
 *  the system's status). While the picker is open its wheel takes the drags. */
static void on_gesture(lv_event_t *e) {
    (void)e;
    if (picker_is_open()) return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT) s_swipe = 1;
    else if (dir == LV_DIR_LEFT) s_swipe = -1;
    else if (dir == LV_DIR_TOP) s_swiped_up = true;
}

/** The city, and the station last played in each city ("st<city>"). */
static void save_station(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    char key[8];
    snprintf(key, sizeof(key), "st%d", s_city);
    nvs_set_u8(h, "city", (uint8_t)s_city);
    nvs_set_u8(h, key, (uint8_t)s_station);
    nvs_commit(h);
    nvs_close(h);
}

/** The last station played in this city (its first, by frequency, the first time). */
static int load_station(int city) {
    nvs_handle_t h;
    uint8_t v = 0;
    char key[8];
    snprintf(key, sizeof(key), "st%d", city_wrap(city));
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, key, &v);
        nvs_close(h);
    }
    return station_step(city, v, 0);
}

static int load_city(void) {
    nvs_handle_t h;
    uint8_t v = 0;  // 北京 the first time
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "city", &v);
        nvs_close(h);
    }
    return city_wrap(v);
}

/** The status line and the rim from what the player and the network are doing. Plain words, no codes. */
static void show_status(void) {
    const char *text = "";
    uint32_t color = YK_INK2;
    bool connecting = false;
    if (s_paused) {
        text = T("已暂停 · 短按继续", "Paused · Tap to resume");
    } else {
        switch (radio_state()) {
        case RADIO_WAIT_NET:
            if (yk_wifi_state() == YK_WIFI_UNCONFIGURED) text = T("没有网络，回到 Yay 设置", "No network. Set it up in Yay"), color = YK_CORAL_TEXT;
            else if (yk_wifi_state() == YK_WIFI_FAILED) text = T("网络连不上，正在重试", "Can't connect. Retrying"), color = YK_CORAL_TEXT;
            else text = T("正在连网", "Connecting"), connecting = true;
            break;
        case RADIO_CONNECTING: text = T("正在调台", "Tuning"), connecting = true; break;
        case RADIO_PLAYING:
            // How to use it for the first seconds of playing: tuning, then cities.
            if (esp_timer_get_time() < s_hint_until_us - HINT_US / 2) text = T("左右滑动调台 · 短按暂停", "Swipe to tune · Tap to pause");
            else if (esp_timer_get_time() < s_hint_until_us) text = T("上滑换城市", "Swipe up for other cities");
            else text = T("正在播放", "Playing");
            color = YK_SAGE_TEXT;
            break;
        case RADIO_BUFFERING: text = T("信号弱，正在缓冲", "Weak signal. Buffering"), connecting = true; break;
        case RADIO_RETRYING: text = T("信号断了，正在重连", "Signal lost. Reconnecting"), color = YK_CORAL_TEXT; break;
        case RADIO_IDLE: break;
        }
    }
    if (strcmp(lv_label_get_text(s_status), text)) lv_label_set_text(s_status, text);
    lv_obj_set_style_text_color(s_status, lv_color_hex(color), 0);
    const yk_rim_mode_t rim = connecting ? YK_RIM_CONNECTING : YK_RIM_OFF;
    if (yk_rim_mode(s_rim) != rim) yk_rim_set(s_rim, rim, NULL);
    const bool live = !s_paused && radio_state() == RADIO_PLAYING;
    tuner_level(live ? yk_speak_level() : 0, live);
}

static void tune(int delta) {
    s_station = station_step(s_city, s_station, delta);
    printf("CITYRADIO_TUNE city=%d station=%d name=\"%s\"\n", s_city, s_station, station_at(s_city, s_station)->name);
    tuner_show_station(s_city, s_station, true);
    save_station();
    s_paused = false;
    radio_play(station_at(s_city, s_station));
}

/** Closes the picker on this city: another city plays the station last played there. */
static void choose_city(int city) {
    picker_close();
    city = city_wrap(city);
    if (city == s_city) return;
    s_city = city;
    s_station = load_station(city);
    printf("CITYRADIO_CITY city=%d name=\"%s\"\n", s_city, city_name(s_city, false));
    tuner_show_station(s_city, s_station, false);
    save_station();
    s_paused = false;
    radio_play(station_at(s_city, s_station));
}

/** LVGL timer: runs with the board lock held, so it may touch the screen. */
static void tick(lv_timer_t *timer) {
    (void)timer;
    const int64_t now = esp_timer_get_time();
    if (radio_state() == RADIO_PLAYING && !s_paused) s_listened_us += now - s_last_tick_us;
    s_last_tick_us = now;
    if (picker_is_open()) {
        // A city tapped; the side key's tap goes back without changing. The player plays on meanwhile.
        const int city = picker_take_choice();
        if (city >= 0) choose_city(city);
        else if (s_tapped) picker_close(), printf("CITYRADIO_PICKER closed\n");
        s_tapped = false;
        s_swipe = 0;
    } else if (s_swiped_up) {
        picker_open(s_city, s_english);
        printf("CITYRADIO_PICKER open\n");
    }
    s_swiped_up = false;
    if (s_tapped) {
        s_tapped = false;
        s_paused = !s_paused;
        printf("CITYRADIO_%s\n", s_paused ? "PAUSE" : "RESUME");
        if (s_paused) radio_stop();
        else radio_play(station_at(s_city, s_station));
    }
    if (s_swipe) {
        const int d = s_swipe;
        s_swipe = 0;
        tune(d);
    }
    show_status();
}

/** Leaving for Yay (a double click, app.exit): a record for the platform and the sentence Yay says when it is
 *  back. Runs in a task of yaykit_app's, cut after YK_APP_LEAVE_MS (400 ms): keep it to writing. */
static void on_leave(void *ctx) {
    (void)ctx;
    const int seconds = (int)(s_listened_us / 1000000);
    if (seconds < 5) return;  // nothing really heard: Yay just comes back
    const station_t *st = station_at(s_city, s_station);
    const int minutes = (seconds + 30) / 60;
    char data[160];  // the record keeps the stations' own (Chinese) names
    snprintf(data, sizeof(data), "{\"station\":\"%s\",\"city\":\"%s\",\"seconds\":%d}", st->name, st->city, seconds);
    const bool recorded = yk_app_record("cityradio.listen", data) == ESP_OK;
    yk_summary_t summary;
    yk_summary_init(&summary);
    char value[12];
    snprintf(value, sizeof(value), "%d", minutes);
    yk_summary_stat(&summary, value, T("分钟", "min"));
    summary.recorded = recorded;
    char sentence[96];
    listen_sentence(st, seconds, s_english, sentence, sizeof(sentence));
    cJSON *doc = cJSON_Parse(data);
    yk_app_set_result_summary(sentence, &summary, doc);
    cJSON_Delete(doc);
}

static esp_err_t on_command(const char *name, const cJSON *args, cJSON *reply, const char **code, void *ctx) {
    (void)ctx;
    yk_app_input();
    esp_err_t err = yk_app_command(name, args, reply, code);  // app.exit, app.recovery, app.status
    if (err != ESP_ERR_NOT_SUPPORTED) return err;
    err = yk_wifi_command(name, args, reply, code);  // wifi.scan, wifi.join: `kit` / yay_dev.py wifi-join
    if (err != ESP_ERR_NOT_FOUND) return err;
    if (!strcmp(name, "cityradio.english")) {  // {"on":true}: the English screen without changing the account
        const cJSON *on = cJSON_GetObjectItem(args, "on");
        board_lock(-1);
        s_english = cJSON_IsTrue(on);
        yk_ui_set_english(s_english);
        tuner_set_english(s_english);
        tuner_show_station(s_city, s_station, false);
        if (picker_is_open()) picker_open(s_city, s_english);
        show_status();
        board_unlock();
        cJSON_AddBoolToObject(reply, "english", s_english);
        return ESP_OK;
    }
    if (!strcmp(name, "cityradio.city")) {  // {"city":2}: as if chosen in the picker; {"picker":true} opens it
        const cJSON *city = cJSON_GetObjectItem(args, "city"), *picker = cJSON_GetObjectItem(args, "picker");
        board_lock(-1);
        if (cJSON_IsNumber(city)) choose_city(city->valueint);
        else if (cJSON_IsTrue(picker)) picker_open(s_city, s_english);
        else if (cJSON_IsFalse(picker)) picker_close();
        cJSON_AddNumberToObject(reply, "city", s_city);
        cJSON_AddStringToObject(reply, "name", city_name(s_city, false));
        cJSON_AddBoolToObject(reply, "picker", picker_is_open());
        board_unlock();
        return ESP_OK;
    }
    if (!strcmp(name, "cityradio.status")) {
        radio_stats_t st;
        radio_stats(&st);
        cJSON_AddStringToObject(reply, "city", city_name(s_city, false));
        cJSON_AddStringToObject(reply, "station", station_at(s_city, s_station)->name);
        cJSON_AddStringToObject(reply, "state", s_paused ? "paused" : radio_state_name(radio_state()));
        cJSON_AddStringToObject(reply, "wifi", yk_wifi_state_name());
        cJSON_AddBoolToObject(reply, "english", s_english);
        cJSON_AddNumberToObject(reply, "listenedSeconds", (double)(s_listened_us / 1000000));
        cJSON_AddNumberToObject(reply, "connects", st.connects);
        cJSON_AddNumberToObject(reply, "failures", st.failures);
        cJSON_AddNumberToObject(reply, "bytes", st.bytes);
        cJSON_AddNumberToObject(reply, "buffered", st.buffered);
        cJSON_AddNumberToObject(reply, "rebuffers", st.rebuffers);
        cJSON_AddNumberToObject(reply, "inRate", st.in_rate);
        cJSON_AddNumberToObject(reply, "decodeErrors", st.decode_errors);
        return ESP_OK;
    }
    *code = "UNKNOWN_COMMAND";
    return ESP_ERR_NOT_SUPPORTED;
}

static int heap_cmd(int argc, char **argv) {
    (void)argc, (void)argv;
    printf("HEAP internal=%u internal_min=%u psram=%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

static void start_console(void) {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = APP_SLUG ">";
    cfg.task_stack_size = 8192;
    esp_console_dev_usb_serial_jtag_config_t usb = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb, &cfg, &repl));
    esp_console_register_help_command();
    ESP_ERROR_CHECK(yk_link_register_console());
    ESP_ERROR_CHECK(yk_app_register_console());
    ESP_ERROR_CHECK(yk_speak_register_console());
    ESP_ERROR_CHECK(yk_wifi_register_console());
    yk_ui_register_snapshot();
    const esp_console_cmd_t heap = {.command = "heap", .help = "available memory", .func = heap_cmd};
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void) {
    // 1. NVS: never erase it (it is shared with every app and holds the Wi-Fi credentials).
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "NVS unavailable (%s); running without saved settings", esp_err_to_name(err));
    // 2. yaykit_app first, so that a crash anywhere below counts towards the crash loop guard (it also subscribes
    //    for the double click home).
    ESP_ERROR_CHECK(yk_app_start(&(yk_app_config_t){.slug = APP_SLUG, .version = APP_VERSION}));
    yk_app_on_leave(on_leave, NULL);
    // 3. Board and screen (dark until the first frame), the speaker at 24 kHz (this app never records), the side key.
    if (!board_display_start_opts(&(board_display_opts_t){.start_dark = true})) {
        ESP_LOGE(TAG, "display failed");
        return;
    }
    board_rtc_init();
    ESP_ERROR_CHECK(yk_speak_init_cfg(&(yk_speak_config_t){.rate = 24000}));
    // delay_tap: the first press of a double click (the app ring) is not a tap; a tap comes 0.45 s after its release.
    ESP_ERROR_CHECK(yk_key_init(&(yk_key_config_t){.delay_tap = true}));
    ESP_ERROR_CHECK(yk_app_on_key(on_key, NULL));
    // 4. Yaykit Link: `yk info` and `yk cmd` over USB (BLE stays off until asked for).
    const yk_link_config_t link = {.board = "tikpal-r1", .app_slug = APP_SLUG, .app_version = APP_VERSION,
                                   .layout = "yaykit-v1", .idle_seconds = 90, .on_command = on_command};
    ESP_ERROR_CHECK(yk_link_init(&link));
    // The owner's language, as the account on the device keeps it (read from NVS only: this app uses no cloud).
    if (yk_account_init(&(yk_account_config_t){.app_slug = APP_SLUG, .app_version = APP_VERSION,
                                               .board = "tikpal-r1"}) == ESP_OK)
        s_english = yk_account_english();
    // 5. Launch parameters from Yay (read once; this version takes none).
    yk_launch_t launch;
    if (yk_app_launch(&launch) == ESP_OK && launch.present) printf("CITY_RADIO_LAUNCH intent=%s params=%s\n", launch.intent, launch.params);
    s_city = load_city();
    s_station = load_station(s_city);

    // 6. The screen; then confirm the image (the bootloader would roll back an app that never gets here).
    board_lock(-1);
    ESP_ERROR_CHECK(cjk_font_init());
    yk_ui_set_english(s_english);  // the shared pages and hints
    tuner_set_english(s_english);
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_style_bg_color(screen, lv_color_hex(YK_PAPER), 0);
    lv_obj_add_event_cb(screen, on_gesture, LV_EVENT_GESTURE, NULL);
    s_rim = yk_rim_create(screen);
    // The round-screen spec's colours and sizes (yk_round.h); text inside the radius-200 circle.
    tuner_create(screen);
    s_status = yk_ui_label(screen, "", 0, 334, 466, YK_SIZE_STATUS, YK_INK2);
    s_hint_until_us = esp_timer_get_time() + HINT_US;
    tuner_show_station(s_city, s_station, false);
    picker_create(screen);  // hidden, over the tuner
    show_status();
    lv_screen_load(screen);
    lv_refr_now(NULL);
    board_brightness_set(board_pref_brightness());  // the device's brightness, set in Yay
    // The first time in this app: an arrow at the side key, 「双击：应用和回家」, until the app ring was opened once.
    if (yk_hint_should(YK_HINT_APPS)) yk_hint_show(screen, YK_HINT_APPS, NULL);
    s_last_tick_us = esp_timer_get_time();
    lv_timer_create(tick, 50, NULL);  // 20 fps for the level bars
    board_unlock();
    // 7. The console, then yk_app_ready(): its banner tells tools the app is up, so the console must already
    //    listen.
    start_console();
    yk_app_ready();
    // 8. The network (the credentials Yay keeps), then the player: it waits for the network by itself.
    err = yk_wifi_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "Wi-Fi did not start (%s)", esp_err_to_name(err));
    ESP_ERROR_CHECK(radio_init());
    radio_play(station_at(s_city, s_station));
}
