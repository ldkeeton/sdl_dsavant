/* Resets the mouse driver (int 33h/0) unless mode is 4, recording the
   button count, and adds 1000h to *result when there is a mouse.  The host
   always has one. */
#include "bios.h"
#include "dshw.h"

detect_mouse(mode, result)
int *result;
{
	if (mode == 4)
		return;
	mouse_button_count = mouse_reset();
	if (mouse_button_count)
		*result |= 0x1000;
}

/* Moves the mouse to pixel (x, y), when there is one. */
mouse_set_pos(x, y)
{
	if (mouse_button_count)
		mouse_move_to(x << 1, y);
}
