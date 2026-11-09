/* The mouse calls every driver shared: position and buttons, clicks
   against a list of rectangles, and gliding the pointer.  On DOS:
   get_mouse.s, clear_mouse_click.s, mouse_hit_test.s,
   request_mouse_redraw.s, update_mouse_pointer.s, mouse_glide_to.s,
   mouse_glide_back.s.  x is in pixels; int 33h's is doubled. */
#include "drv.h"
#include "bios.h"

/* Buttons (1 left, 2 right); the position in *x, *y. */
get_mouse(x, y)
int *x, *y;
{
	uint16_t mx, my, buttons;

	if (!mouse_present)
		return 0;
	buttons = mouse_get(&mx, &my);
	*x = mx >> 1;
	*y = my;
	return buttons;
}

clear_mouse_click()
{
	mouse_click_pending = 0;
}

/* Which of rects ({x1, y1, x2, y2} inclusive, ending in x1 = -1) holds
   the pending click, as -(its number from 1), or else the pointer, as
   +its number; 0 if none.  Takes the click. */
mouse_hit_test(rects)
int *rects;
{
	unsigned buttons, x, y, n;
	uint16_t mx, my;

	if (!mouse_present)
		return 0;
	if ((buttons = mouse_click_pending) != 0) {
		mouse_click_pending = 0;
		x = mouse_click_x;
		y = mouse_click_y;
	} else {
		mouse_get(&mx, &my);
		x = mx >> 1;
		y = my;
	}
	for (n = 1; rects[0] != -1; rects += 4, n++)
		if (x >= (unsigned)rects[0] && y >= (unsigned)rects[1] &&
		    (unsigned)rects[2] >= x && (unsigned)rects[3] >= y)
			return buttons ? -n : n;
	return 0;
}

/* Between these two (around a screen update on DOS), the interrupt
   handler leaves the pointer alone. */
request_mouse_redraw()
{
	if (mouse_present == 1)
		mouse_redraw_pending = 1;
}

update_mouse_pointer()
{
	uint16_t x, y;

	if (!mouse_glide_active && mouse_present == 1) {
		mouse_get(&x, &y);
		if (mouse_drawn_x != x >> 1 || mouse_drawn_y != y)
			draw_mouse_pointer_at(x, y);
	}
	mouse_redraw_pending = 0;
}

/* Moves the pointer a step at a time from (*x, *y) to (tx, ty), int 33h
   units, waiting delay ms a step. */
static
glide(x, y, tx, ty, delay)
unsigned x, y, tx, ty, delay;
{
	while (x != tx || y != ty) {
		if (x != tx)
			x += (int)(x - tx) < 0 ? 2 : -2;
		if (y != ty)
			y += (int)(y - ty) < 0 ? 1 : -1;
		draw_mouse_pointer_at(x, y);
		mouse_move_to(x, y);
		if (delay)
			far_delay_ms(delay);
	}
}

/* Glides the pointer to (x, y) and holds it there (the mouse no longer
   moves it) until mouse_glide_back. */
mouse_glide_to(x, y, delay)
unsigned x, y;
{
	uint16_t mx, my;

	if (mouse_present != 1)
		return;
	mouse_glide_active = 1;
	mouse_get(&mx, &my);
	glide(mx & 0xfffe, my, x << 1, y, delay);
}

/* Glides it from where it was held to (x, y), and lets go. */
mouse_glide_back(x, y, delay)
unsigned x, y;
{
	if (mouse_present != 1)
		return;
	glide(mouse_drawn_x << 1, mouse_drawn_y, x << 1, y, delay);
	mouse_move_to(x << 1, y);
	mouse_glide_active = 0;
}
