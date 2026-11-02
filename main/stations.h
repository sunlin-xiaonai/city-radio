// The cities, their stations and what is said about them. Plain C, no ESP-IDF: host tested (test/run.sh).
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define FM_MIN_10 875   // the dial's ends, in 0.1 MHz
#define FM_MAX_10 1080

typedef struct {
    const char *city;     // 北京
    const char *city_en;  // Beijing (short: it runs along the circle)
    const char *name;     // 北京交通广播
    const char *name_en;  // Beijing Traffic
    int mhz10;         // 1039 = FM 103.9
    const char *url;   // an HTTP MP3 stream
} station_t;

int cities_count(void);
/** A city index wrapped into the list (any int). */
int city_wrap(int city);
/** The city's name in the screen's language. */
const char *city_name(int city, bool english);
/** How many stations the city has (at least two). */
int city_stations(int city);
/** The city's station at i, both wrapped; a city's stations run low to high in frequency. */
const station_t *station_at(int city, int i);
/** i moved by delta within the city, wrapped: the next (+1) or previous (-1) station. */
int station_step(int city, int i, int delta);
/** 「103.9」 */
void station_freq_text(const station_t *st, char *out, size_t size);
/** Where a frequency sits on the dial, 0..1000 (clamped to FM_MIN_10..FM_MAX_10). */
int dial_permille(int mhz10);
/** The city and the station's name in the screen's language. */
const char *station_city(const station_t *st, bool english);
const char *station_name(const station_t *st, bool english);
/** The sentence Yay says when it is back: 「听了北京交通广播 25 分钟」, under a minute 「听了一会儿北京交通广播」;
 *  in English "Listened to Beijing Traffic for 25 minutes", "Listened to Beijing Traffic for a moment". */
void listen_sentence(const station_t *st, int seconds, bool english, char *out, size_t size);
