/* The VGA mouse pointer.  VGA.DRV drew it into the display itself, saving
   what was under it and taking it off around every show_area; here the
   host draws it over the display at the mouse position (bios.h), so all
   that is left is which image, and where the driver thinks it is
   (mouse_drawn_x/y, which a glide moves).  On DOS: mouse_init.s,
   mouse_off.s, set_mouse_pointer.s, mouse_event_handler.s,
   redraw_mouse_pointer.s, clip_mouse_pointer_to_area.s and vga/data. */
#include "drv.h"
#include "bios.h"

/* 10x10, 0FFh transparent. */
static unsigned char mouse_default_cursor_image[100] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,
	0x00,0xb3,0xb6,0xb6,0xb6,0xb6,0x00,0xff,0xff,0xff,
	0x00,0xb3,0x13,0x15,0xb6,0x00,0xff,0xff,0xff,0xff,
	0x00,0xb3,0x15,0x16,0xb6,0x00,0xff,0xff,0xff,0xff,
	0x00,0xb3,0xb6,0xb3,0xb6,0xc1,0x00,0xff,0xff,0xff,
	0x00,0xb3,0x00,0x00,0xb3,0xb6,0xc1,0x00,0xff,0xff,
	0x00,0x00,0xff,0xff,0x00,0xb3,0xb6,0xc1,0x00,0xff,
	0xff,0xff,0xff,0xff,0xff,0x00,0xb3,0xb6,0xc1,0x00,
	0xff,0xff,0xff,0xff,0xff,0xff,0x00,0xb3,0xc3,0x00,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0xff
};

static unsigned char *mouse_image;	/* 0: the default */
static unsigned mouse_image_stride = 10, mouse_image_height = 10;

/* The pointer is now at (x / 2, y) (x in int 33h units). */
draw_mouse_pointer_at(x, y)
unsigned x, y;
{
	mouse_drawn_x = x >> 1;
	mouse_drawn_y = y;
	mouse_pointer_image(mouse_image ? mouse_image : mouse_default_cursor_image,
		mouse_image_stride, mouse_image_height, mouse_image_stride);
}

/* int 33h's handler for moves and left and right presses (on the host's
   event thread, as it was in an interrupt): a press is kept for
   mouse_hit_test. */
void
mouse_event_handler(uint16_t events, uint16_t buttons, uint16_t x, uint16_t y)
{
	if (events & 0x0a) {
		mouse_click_pending = buttons;
		mouse_click_x = x >> 1;
		mouse_click_y = y;
		if (x >> 1 == mouse_drawn_x && y == mouse_drawn_y)
			return;
	}
	if (mouse_redraw_pending || mouse_glide_active)
		return;
	draw_mouse_pointer_at(x, y);
}

mouse_init()
{
	uint16_t x, y;

	if (!mouse_init_enabled)
		return;
	mouse_reset();
	mouse_set_range_x(0, 639);
	mouse_set_range_y(0, 199);
	mouse_move_to(0, 0);
	mouse_get(&x, &y);
	draw_mouse_pointer_at(x, y);
	mouse_pointer_show(1);
	mouse_present = 1;
	mouse_set_handler(0x0b, mouse_event_handler);
}

/* The pointer off and the mouse driver reset (no handler). */
mouse_off()
{
	if (mouse_present) {
		mouse_present = 0;
		mouse_reset();
	}
}

/* A new pointer image (0: the default), stride x height pixels; then the
   pointer at (x, y), or where it is if either is -1. */
set_mouse_pointer(image, stride, height, x, y)
char *image;
unsigned stride, height;
int x, y;
{
	uint16_t mx, my;

	if (stride >= 0x51 || (long)stride * height >= 0x401)
		dos_fatal("\r\n\007Mouse pointer size out of range.\n$");
	request_mouse_redraw();
	mouse_image = (unsigned char *)image;
	mouse_image_stride = stride;
	mouse_image_height = height;
	if (!image)
		mouse_image_stride = mouse_image_height = 10;
	if (!mouse_present)
		return;
	if (x != -1 && y != -1)
		mouse_move_to(x << 1, y);
	else if (mouse_glide_active) {
		draw_mouse_pointer_at(mouse_drawn_x << 1, mouse_drawn_y);
		mouse_redraw_pending = 0;
		return;
	}
	mouse_get(&mx, &my);
	draw_mouse_pointer_at(mx, my);
	mouse_redraw_pending = 0;
}
