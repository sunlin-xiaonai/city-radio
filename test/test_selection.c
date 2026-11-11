#include <assert.h>
#include <stdio.h>
#include "../main/selection.h"

int main(void) {
    selection_cache_t c = {0};
    selection_snapshot_t s;
    selection_load(&c, 1, 2);
    assert(selection_get(&c, 1) == 2);
    selection_select(&c, 1, 3, 100);
    selection_select(&c, 1, 4, 200);
    selection_select(&c, 2, 5, 300);
    assert(selection_get(&c, 1) == 4 && selection_get(&c, 2) == 5);
    assert(!selection_due(&c, 120000099));
    assert(selection_due(&c, 120000100));
    assert(selection_snapshot(&c, &s));
    assert(s.city == 2 && s.stations[1] == 4 && s.stations[2] == 5);
    selection_finish(&c, &s, false);
    assert(selection_snapshot(&c, &s));
    selection_select(&c, 1, 6, 120000200);
    selection_finish(&c, &s, true);
    assert(selection_snapshot(&c, &s));
    assert(s.city == 1 && s.stations[1] == 6 && !(s.dirty & (1u << 2)));
    selection_finish(&c, &s, true);
    assert(!selection_snapshot(&c, &s));
    puts("selection cache tests passed");
}
