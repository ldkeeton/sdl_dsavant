/* Between the SDL main thread (platform.c) and the emulated hardware
   (bios.c).  Host code only: it includes SDL. */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <SDL3/SDL.h>

#define SCREEN_W 320
#define SCREEN_H 200

/* bios.c, at startup: start the tick timer, set up the locks. */
void bios_init(void);

/* bios.c, on the SDL thread: a key went down (repeats included). */
void bios_key_event(const SDL_KeyboardEvent *e);
/* bios.c, on the SDL thread: the mouse is at (x, y) in screen pixels
   (0..319, 0..199, fractions allowed); a button changed. */
void bios_mouse_motion(float x, float y);
void bios_mouse_button(uint8_t sdl_button, int down);

/* What the presenter needs to draw the pointer.  Returns 0 when there is
   none to draw. */
struct pointer_state {
    const uint8_t *image;
    int w, h, stride;
    int x, y;                   /* screen pixels, top left */
};
int bios_pointer(struct pointer_state *p);

/* A warp the game asked for (mouse_set_pos), in screen pixels: taken by
   the SDL thread, which moves the host cursor.  Returns 0 if none. */
int bios_take_warp(float *x, float *y);

/* platform.c: wake the SDL thread to handle a request now. */
void platform_wake(void);

#endif
