// Host tests for City Radio's plain C parts: the station list and the PCM conversion. Run: bash test/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../main/pcm_conv.h"
#include "../main/spectrum.h"
#include "../main/stations.h"

static int failures, checks;
#define CHECK(cond)                                                        \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                  \
    } while (0)

static void test_stations(void) {
    const int cities = cities_count();
    CHECK(cities >= 10 && cities <= 255);  // the city is kept in a byte
    CHECK(city_wrap(cities) == 0 && city_wrap(-1) == cities - 1);
    for (int c = 0; c < cities; c++) {
        const int n = city_stations(c);
        CHECK(n >= 2 && n <= CITY_MAX_STATIONS);
        CHECK(station_step(c, n - 1, 1) == 0);
        CHECK(station_step(c, 0, -1) == n - 1);
        CHECK(station_at(c, n) == station_at(c, 0));
        CHECK(station_at(c, -1) == station_at(c, n - 1));
        CHECK(!strcmp(city_name(c, false), station_at(c, 0)->city));
        CHECK(!strcmp(city_name(c, true), station_at(c, 0)->city_en));
        for (int d = 0; d < c; d++) CHECK(strcmp(city_name(c, false), city_name(d, false)));  // each city once
        for (int i = 0; i < n; i++) {
            const station_t *s = station_at(c, i);
            CHECK(s->city && s->name && s->city_en && s->name_en && !strncmp(s->url, "http://", 7));
            CHECK(!strcmp(s->city, city_name(c, false)));  // a city's stations are all in it
            CHECK(strlen(s->city_en) <= 9);  // along the circle: short (Guangzhou, Chongqing are the longest)
            CHECK(s->mhz10 >= FM_MIN_10 && s->mhz10 <= FM_MAX_10);
            if (i) CHECK(s->mhz10 >= station_at(c, i - 1)->mhz10 + CITY_MIN_GAP_10);  // low to high, peaks apart
            for (int d = 0; d < c; d++)  // no stream twice
                for (int j = 0; j < city_stations(d); j++) CHECK(strcmp(s->url, station_at(d, j)->url));
        }
    }
    char text[96];
    const station_t st = {"北京", "Beijing", "北京交通广播", "Beijing Traffic", 1039, "http://x"};
    station_freq_text(&st, text, sizeof(text));
    CHECK(!strcmp(text, "103.9"));
    CHECK(dial_permille(FM_MIN_10) == 0 && dial_permille(FM_MAX_10) == 1000);
    CHECK(dial_permille(800) == 0 && dial_permille(1200) == 1000);
    CHECK(dial_permille((FM_MIN_10 + FM_MAX_10) / 2) >= 495 && dial_permille((FM_MIN_10 + FM_MAX_10) / 2) <= 500);
    CHECK(!strcmp(station_city(&st, false), "北京") && !strcmp(station_city(&st, true), "Beijing"));
    CHECK(!strcmp(station_name(&st, false), "北京交通广播") && !strcmp(station_name(&st, true), "Beijing Traffic"));
    listen_sentence(&st, 25 * 60 + 10, false, text, sizeof(text));
    CHECK(!strcmp(text, "听了北京交通广播 25 分钟"));
    listen_sentence(&st, 89, false, text, sizeof(text));
    CHECK(!strcmp(text, "听了北京交通广播 1 分钟"));
    listen_sentence(&st, 20, false, text, sizeof(text));
    CHECK(!strcmp(text, "听了一会儿北京交通广播"));
    listen_sentence(&st, 25 * 60 + 10, true, text, sizeof(text));
    CHECK(!strcmp(text, "Listened to Beijing Traffic for 25 minutes"));
    listen_sentence(&st, 89, true, text, sizeof(text));
    CHECK(!strcmp(text, "Listened to Beijing Traffic for 1 minute"));
    listen_sentence(&st, 20, true, text, sizeof(text));
    CHECK(!strcmp(text, "Listened to Beijing Traffic for a moment"));
}

/** The dial's signal line: each station's peak is its own, and the pieces of the line meet. */
static void test_spectrum(void) {
    CHECK(spec_step(FM_MIN_10) == 0 && spec_step(FM_MAX_10) == SPEC_STEPS - 1);
    CHECK(spec_step(800) == 0 && spec_step(1200) == SPEC_STEPS - 1);
    // A peak is under a pixel past its half window: the line drawn without it joins the one drawn with it.
    CHECK(spec_peak(1000, SPEC_LIVE, spec_step(1000) + SPEC_HALF + 1) < 0.5f);
    CHECK(spec_peak(1000, SPEC_LIVE, spec_step(1000) - SPEC_HALF - 1) < 0.5f);
    CHECK(spec_peak(1000, SPEC_LIVE, spec_step(1000)) == SPEC_LIVE);
    for (int c = 0; c < cities_count(); c++)
        for (int s = 0; s < city_stations(c); s++) {
            const int at = spec_step(station_at(c, s)->mhz10);
            // The highest point of the line is at the station's own frequency.
            const float top = spec_height(c, -1, at);
            CHECK(top >= SPEC_REST && top <= SPEC_REST + 2);
            CHECK(spec_height(c, -1, at - 2) < top && spec_height(c, -1, at + 2) < top);
            // In the playing station's window (drawn coral) the other stations add under a pixel.
            for (int i = at - SPEC_HALF; i <= at + SPEC_HALF; i++)
                if (i >= 0 && i < SPEC_STEPS) CHECK(spec_height(c, s, i) < 1.0f + 0.5f);
        }
}

/** Converts `frames` stereo frames of a ramp in blocks of `block`; returns the output count. */
static size_t convert_ramp(uint32_t in_rate, size_t frames, size_t block, int16_t *out, size_t cap) {
    pcm_conv_t c;
    pcm_conv_init(&c, in_rate, 24000);
    int16_t *in = malloc(frames * 2 * sizeof(int16_t));
    for (size_t i = 0; i < frames; i++) in[2 * i] = in[2 * i + 1] = (int16_t)(i % 20000);
    size_t n = 0;
    for (size_t at = 0; at < frames; at += block) {
        const size_t len = frames - at < block ? frames - at : block;
        n += pcm_conv_run(&c, in + 2 * at, len, 2, out + n, cap - n);
    }
    free(in);
    return n;
}

static void test_pcm(void) {
    enum { CAP = 30000 };
    static int16_t whole[CAP], blocks[CAP];
    // 48 kHz -> 24 kHz: exactly half, every other input sample (one input sample late: the first output is the
    // primed sample).
    size_t n = convert_ramp(48000, 48000, 48000, whole, CAP);
    CHECK(n == 24000);
    CHECK(whole[0] == 0 && whole[1] == 1 && whole[100] == 199);
    // 44.1 kHz -> 24 kHz: a second in is a second out (within a sample).
    n = convert_ramp(44100, 44100, 44100, whole, CAP);
    CHECK(n >= 23999 && n <= 24001);
    // Block by block (an MP3 frame is 1152) gives the same samples as all at once.
    const size_t m = convert_ramp(44100, 44100, 1152, blocks, CAP);
    CHECK(m == n && !memcmp(whole, blocks, n * sizeof(int16_t)));
    // 32 kHz (湖北之声, 楚天交通): a second in is a second out too.
    n = convert_ramp(32000, 32000, 1152, whole, CAP);
    CHECK(n >= 23999 && n <= 24001);
    // Stereo is averaged to mono; a constant stays constant.
    pcm_conv_t c;
    pcm_conv_init(&c, 44100, 24000);
    int16_t lr[2 * 441], out[300];
    for (int i = 0; i < 441; i++) lr[2 * i] = 1000, lr[2 * i + 1] = -200;
    n = pcm_conv_run(&c, lr, 441, 2, out, 300);
    CHECK(n >= 239 && n <= 241);
    int same = 1;
    for (size_t i = 0; i < n; i++) same &= out[i] == 400;
    CHECK(same);
    CHECK(pcm_conv_max_out(&c, 1152) >= 1152 * 24000 / 44100 + 1);
    // A mono stream (长沙新闻广播) passes through as it is, only resampled.
    pcm_conv_init(&c, 44100, 24000);
    int16_t mono_in[441];
    for (int i = 0; i < 441; i++) mono_in[i] = 700;
    n = pcm_conv_run(&c, mono_in, 441, 1, out, 300);
    same = n >= 239 && n <= 241;
    for (size_t i = 0; i < n; i++) same &= out[i] == 700;
    CHECK(same);
    // A full output buffer drops the rest of the block but stays in step for the next one.
    pcm_conv_init(&c, 48000, 24000);
    n = pcm_conv_run(&c, lr, 441, 2, out, 10);
    CHECK(n == 10);
    n = pcm_conv_run(&c, lr, 441, 2, out, 300);
    CHECK(n >= 220 && n <= 221);
}

int main(void) {
    test_stations();
    test_spectrum();
    test_pcm();
    printf("city-radio host tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
