#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SELECTION_CITIES 32
#define SELECTION_INTERVAL_US 120000000LL

typedef struct {
    uint8_t stations[SELECTION_CITIES];
    uint32_t loaded, dirty;
    uint8_t city;
    bool city_dirty;
    int64_t dirty_since;
} selection_cache_t;

typedef struct {
    uint8_t stations[SELECTION_CITIES];
    uint32_t dirty;
    uint8_t city;
    bool city_dirty;
} selection_snapshot_t;

int selection_get(const selection_cache_t *c, int city);
void selection_load(selection_cache_t *c, int city, int station);
void selection_select(selection_cache_t *c, int city, int station, int64_t now);
bool selection_due(const selection_cache_t *c, int64_t now);
bool selection_snapshot(const selection_cache_t *c, selection_snapshot_t *s);
void selection_finish(selection_cache_t *c, const selection_snapshot_t *s, bool success);
