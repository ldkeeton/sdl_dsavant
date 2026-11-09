/* The VGA side of the screen: the off-screen buffer onto the display, and
   the buffer operations VGA.DRV kept for itself.  On DOS:
   fill_screen_buffer.s, show_area.s, invert_area.s, set_text_color.s,
   get_text_pos.s.  The mouse pointer is the host's (drawn over the
   display), so show_area no longer takes it off and puts it back. */
#include "drv.h"
#include "hostlib.h"

#define SCREEN(off)	(*DOS_PTR(screen_buf_seg, off))

fill_screen_buffer(c)
{
	host_memset(SCREEN_BUF, c, 64000);
}

/* Copies (x1, y1)-(x2, y2), inclusive, from the buffer to the display. */
show_area(x1, y1, x2, y2)
unsigned x1, y1, x2, y2;
{
	unsigned off, cols, rows;

	off = y1 * 320 + x1;
	cols = x2 - x1 + 1;
	rows = y2 - y1 + 1;
	do {
		host_memmove(DOS_PTR(VGA_SEGMENT, off), DOS_PTR(screen_buf_seg, off), cols);
		off += 320;
	} while (--rows);
}

/* Complements (x1, y1)-(x2, y2), inclusive, in the buffer. */
invert_area(x1, y1, x2, y2)
unsigned x1, y1, x2, y2;
{
	unsigned off, rows, cols, n, i;

	off = y1 * 320 + x1;
	rows = y2 - y1 + 1;
	cols = x2 - x1 + 1;
	do {
		i = off;
		n = cols;
		do {
			SCREEN(i) = ~SCREEN(i);
			i++;
		} while (--n);
		off += 320;
	} while (--rows);
}

set_text_color(fg, bg)
{
	primary_text_color = fg;
	secondary_text_color = bg;
}

get_text_pos(x, y)
int *x, *y;
{
	*x = text_pos_x;
	*y = text_pos_y;
}
