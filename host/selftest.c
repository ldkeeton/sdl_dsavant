/* DS_SELFTEST=1: exercises the platform on the game thread, in place of the
   game.  Draws every palette colour, a bar that moves with the BIOS tick and
   the mouse pointer; reports keys, clicks, file and memory calls on stderr.
   Esc quits. */
#include <stdio.h>
#include <string.h>
#include "bios.h"
#include "dosfile.h"
#include "dosmem.h"

static const uint8_t arrow[8 * 8] = {
#define _ 0xff
    15, _, _, _, _, _, _, _,
    15, 15, _, _, _, _, _, _,
    15, 0, 15, _, _, _, _, _,
    15, 0, 0, 15, _, _, _, _,
    15, 0, 0, 0, 15, _, _, _,
    15, 0, 15, 15, 15, 15, _, _,
    15, 15, _, _, _, _, _, _,
    15, _, _, _, _, _, _, _,
#undef _
};

static void on_mouse(uint16_t events, uint16_t buttons, uint16_t x, uint16_t y)
{
    if (events & 0x0a)
        fprintf(stderr, "click events=%02x buttons=%d at %d,%d\n", events, buttons, x / 2, y);
}

static void files(void)
{
    uint8_t buf[16];
    int32_t h = dos_open("c:\\dsavant\\misc.hdr", 0);
    int32_t n = h >= 0 ? dos_read((uint16_t)h, buf, sizeof buf) : 0;
    int32_t size = h >= 0 ? dos_lseek((uint16_t)h, 0, 2) : 0;
    fprintf(stderr, "open misc.hdr: handle %d, read %d, size %d\n", h, n, size);
    if (h >= 0)
        dos_close((uint16_t)h);
    fprintf(stderr, "open nosuch.fil: %d (want -2)\n", dos_open("NOSUCH.FIL", 0));

    h = dos_create("selftest.tmp", 0);
    dos_write((uint16_t)h, "hello", 5);
    dos_lseek((uint16_t)h, 0, 0);
    memset(buf, 0, sizeof buf);
    n = dos_read((uint16_t)h, buf, 5);
    dos_close((uint16_t)h);
    fprintf(stderr, "create/write/read: %d \"%s\", unlink %d\n", n, buf, dos_unlink("SELFTEST.TMP"));
}

static void memory(void)
{
    uint16_t largest = 0, total, free_pages;
    dos_alloc(0xffff, &largest);
    int32_t a = dos_alloc(0x4180, 0), b = dos_alloc(0x100, 0);
    dos_free((uint16_t)a);
    int32_t c = dos_alloc(0x100, 0);
    fprintf(stderr, "dos: largest %04x paras; maze %04x, b %04x, c %04x (reuses maze's)\n",
            largest, a, b, c);
    dos_free((uint16_t)b);
    dos_free((uint16_t)c);
    int32_t e = ems_alloc(8);
    ems_page_counts(&total, &free_pages);
    fprintf(stderr, "ems: handle %d, %d of %d pages free\n", e, free_pages, total);
    ems_free((uint16_t)e);
}

void host_selftest(void)
{
    uint8_t *fb = DOS_PTR(VGA_SEGMENT, 0);

    files();
    memory();

    vga_set_mode(0x13);
    for (int i = 16; i < 256; i++) {
        vga_dac[i][0] = (uint8_t)((i * 7) & 63);
        vga_dac[i][1] = (uint8_t)((i * 3) & 63);
        vga_dac[i][2] = (uint8_t)(i & 63);
    }
    for (int y = 0; y < 160; y++)
        for (int x = 0; x < 320; x++)
            fb[y * 320 + x] = (uint8_t)((y / 10) * 16 + x / 20);

    fprintf(stderr, "buttons: %d\n", mouse_reset());
    mouse_set_range_x(0, 639);
    mouse_set_range_y(0, 199);
    mouse_set_pos(320, 100);
    mouse_pointer_image(arrow, 8, 8, 8);
    mouse_pointer_show(1);
    mouse_set_handler(0x0b, on_mouse);

    int32_t t0 = BIOS_TICKS;
    host_delay_ms(1000);
    fprintf(stderr, "ticks in 1 s: %d (want 18)\n", BIOS_TICKS - t0);

    for (int i = 0; i < 70; i++) {     /* a one-second fade of colour 15 */
        vga_wait_retrace();
        vga_dac[15][0] = vga_dac[15][1] = vga_dac[15][2] = (uint8_t)(i * 63 / 69);
    }

    for (;;) {
        int32_t k = bios_key_peek();
        if (k >= 0) {
            uint8_t c = dos_getch();
            if (c == 0)
                fprintf(stderr, "key: extended, scan %02x\n", dos_getch());
            else
                fprintf(stderr, "key: %02x '%c'\n", c, c >= 32 && c < 127 ? c : '.');
            if (c == 27)
                host_exit(0);
        }
        int pos = (BIOS_TICKS % 64) * 5;
        for (int y = 170; y < 190; y++)
            for (int x = 0; x < 320; x++)
                fb[y * 320 + x] = x >= pos && x < pos + 5 ? 15 : 1;
    }
}
