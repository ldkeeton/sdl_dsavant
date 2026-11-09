/* The PC hardware and BIOS the game and drivers used, emulated by the host
   (bios.c; the SDL main thread plays the hardware's part).

   Callable from game C and its .s replacements.  The SDL side of each piece
   (feeding keys, mouse moves, drawing the screen) is in platform.h. */
#ifndef BIOS_H
#define BIOS_H

#include "hoststd.h"
#include "dosmem.h"

/* ---- timer ------------------------------------------------------------------
   The BIOS tick counter at 0040:006C (dos_mem), counting at 18.2065 Hz from
   startup, kept current by a host timer: code that spins on it sees it
   change. */
#define BIOS_TICK_ADDR  0x46c
#define BIOS_TICKS      (*(volatile int32_t *)(dos_mem + BIOS_TICK_ADDR))

/* Sleep ms milliseconds (delay_ms; calibrate_loops_per_ms is moot). */
void host_delay_ms(uint32_t ms);

/* ---- keyboard ---------------------------------------------------------------
   A BIOS key buffer of scan<<8 | ascii words.  An extended key (cursor
   keys, F-keys, Alt+letter) has ascii 0. */

/* int 16h/1: the next key without taking it, or -1 if none. */
int32_t bios_key_peek(void);
/* int 16h/0: take the next key, waiting for one. */
uint16_t bios_key_read(void);
/* DOS 07h: unechoed character input, waiting.  An extended key comes back
   as 0, and the next call returns its scan code. */
uint8_t dos_getch(void);
/* Empty the key buffer. */
void bios_key_flush(void);

/* ---- mouse (int 33h) --------------------------------------------------------
   Coordinates are the driver's for mode 13h: x is 0..639 (two per pixel),
   y is 0..199.  Buttons: 1 left, 2 right, 4 middle. */

/* Fn 0: reset; returns the number of buttons (0: no mouse). */
uint16_t mouse_reset(void);
/* Fn 3: buttons; position in *x, *y. */
uint16_t mouse_get(uint16_t *x, uint16_t *y);
/* Fn 4: move the pointer (the host cursor follows). */
void mouse_move_to(uint16_t x, uint16_t y);
/* Fn 7 / 8: the range the position is clamped to. */
void mouse_set_range_x(uint16_t min, uint16_t max);
void mouse_set_range_y(uint16_t min, uint16_t max);

/* Fn 0Ch/14h: install handler for the events in mask (1 moved, 2 left
   pressed, 4 left released, 8 right pressed, 16 right released).  It is
   called on the SDL thread, as an interrupt was, with the events that
   happened and the state after them.  0 removes it. */
typedef void (*mouse_handler_fn)(uint16_t events, uint16_t buttons, uint16_t x, uint16_t y);
void mouse_set_handler(uint16_t mask, mouse_handler_fn handler);

/* The pointer image the presenter draws over the screen at the mouse
   position (x/2, y): w x h pixels, stride bytes a row, 0xFF transparent.
   The image is read while the screen is drawn, so it must stay valid. */
void mouse_pointer_image(const uint8_t *image, uint16_t w, uint16_t h, uint16_t stride);
void mouse_pointer_show(int on);

/* ---- VGA --------------------------------------------------------------------
   The framebuffer is dos_mem at A000:0000 (320x200 bytes in mode 13h).  The
   DAC has 256 entries of 6-bit red, green, blue. */
extern uint8_t vga_dac[256][3];

/* int 10h/00h. */
void vga_set_mode(uint8_t mode);
/* int 10h/0Fh: the current mode. */
uint8_t vga_get_mode(void);
/* Wait for the start of the next vertical retrace (70 Hz, as the port 3DAh
   loops did). */
void vga_wait_retrace(void);

/* ---- program ----------------------------------------------------------------
   int 21h/4Ch: end the program with this exit code.  Does not return. */
void host_exit(int code);

#endif
