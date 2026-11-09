/* Empties key_queue and the BIOS key buffer, then clears the driver's
   pending mouse click. */
#include "bios.h"

extern struct {
	unsigned count;
	unsigned char keys[22];
} key_queue;

flush_keyboard()
{
	key_queue.count = 0;
	bios_key_flush();
	clear_mouse_click();
}
