/* Overlays linked into DS (tools/ovlink.py makes the table). */
#ifndef OVERLAY_H
#define OVERLAY_H

#pragma pack(push)
#pragma pack()      /* host16.h packs game structs to 2 */
struct overlay_entry {
    const char *name;
    short (*main)(void);
    char *data, *data_end;      /* initialized data, put back on each load */
    char *bss, *bss_end;        /* zeroed on each load */
};
#pragma pack(pop)

extern const struct overlay_entry overlay_table[];

/* Save every overlay's initial data; call once before the game starts. */
void overlay_init(void);
/* Reset the named overlay's data and run it; returns what ovmain returns.
   Exits the program when there is no such overlay. */
short overlay_run(const char *name);

#endif
