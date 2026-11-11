#include "selection.h"
#include <string.h>

int selection_get(const selection_cache_t *c, int city) {
    if (city < 0 || city >= SELECTION_CITIES || !(c->loaded & (1u << city))) return -1;
    return c->stations[city];
}

void selection_load(selection_cache_t *c, int city, int station) {
    if (city < 0 || city >= SELECTION_CITIES || (c->loaded & (1u << city))) return;
    c->loaded |= 1u << city;
    c->stations[city] = (uint8_t)station;
}

void selection_select(selection_cache_t *c, int city, int station, int64_t now) {
    if (city < 0 || city >= SELECTION_CITIES) return;
    if (!c->dirty && !c->city_dirty) c->dirty_since = now;
    c->city = (uint8_t)city;
    c->city_dirty = true;
    c->stations[city] = (uint8_t)station;
    c->loaded |= 1u << city;
    c->dirty |= 1u << city;
}

bool selection_due(const selection_cache_t *c, int64_t now) {
    return (c->dirty || c->city_dirty) && now - c->dirty_since >= SELECTION_INTERVAL_US;
}

bool selection_snapshot(const selection_cache_t *c, selection_snapshot_t *s) {
    if (!c->dirty && !c->city_dirty) return false;
    memcpy(s->stations, c->stations, sizeof(s->stations));
    s->dirty = c->dirty;
    s->city = c->city;
    s->city_dirty = c->city_dirty;
    return true;
}

void selection_finish(selection_cache_t *c, const selection_snapshot_t *s, bool success) {
    if (!success) return;
    for (int i = 0; i < SELECTION_CITIES; ++i)
        if ((s->dirty & (1u << i)) && c->stations[i] == s->stations[i]) c->dirty &= ~(1u << i);
    if (s->city_dirty && c->city == s->city) c->city_dirty = false;
}
