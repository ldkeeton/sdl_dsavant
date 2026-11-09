/* Overlay loading: an overlay starts each load with fresh data, as when DOS
   read it from disk. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "overlay.h"

static char **snapshots;

void overlay_init(void)
{
    size_t n = 0;
    while (overlay_table[n].name)
        n++;
    snapshots = calloc(n, sizeof *snapshots);
    for (size_t i = 0; i < n; i++) {
        const struct overlay_entry *e = &overlay_table[i];
        size_t size = e->data && e->data_end > e->data ? (size_t)(e->data_end - e->data) : 0;
        snapshots[i] = malloc(size ? size : 1);
        if (size)
            memcpy(snapshots[i], e->data, size);
    }
}

short overlay_run(const char *name)
{
    for (size_t i = 0; overlay_table[i].name; i++) {
        const struct overlay_entry *e = &overlay_table[i];
        if (strcasecmp(e->name, name) != 0)
            continue;
        if (e->data && e->data_end > e->data)
            memcpy(e->data, snapshots[i], (size_t)(e->data_end - e->data));
        if (e->bss && e->bss_end > e->bss)
            memset(e->bss, 0, (size_t)(e->bss_end - e->bss));
        return e->main();
    }
    fprintf(stderr, "Error loading overlay: %s\n", name);
    exit(100);
}
