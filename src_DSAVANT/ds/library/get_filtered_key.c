/* get_filtered_key(mode), poll_filtered_key(mode): the filtered keyboard.

   Keys go through key_queue, with a..z folded to upper case and the
   cursor keys turned into 8 (left), 9 (up), 10 (right) and 11 (down); other
   extended keys are dropped.  Mode 0 lets any key through; mode 2 only BS,
   ESC, CR and digits; any other mode BS, the cursor codes, ESC, CR and
   32..95.  A queued key that does not pass is discarded.

   Taking a key only decrements the count, without moving the rest of the
   queue up, as the original did.  The queue never holds more than the one
   key that was just read, so that is harmless. */
#include "bios.h"

extern struct {
	unsigned count;
	unsigned char keys[22];
} key_queue;

static
passes(mode, c)
unsigned char c;
{
	if ((mode & 0xff) == 0)
		return 1;
	if ((mode & 0xff) == 2)
		return c == 8 || c == 27 || c == 13 || c >= '0' && c <= '9';
	return c >= 8 && c <= 11 || c == 27 || c == 13 || c >= 32 && c < 96;
}

/* Drop key_queue's oldest key, moving the next 20 up. */
static
drop_key()
{
	int i;

	--key_queue.count;
	for (i = 0; i < 20; i++)
		key_queue.keys[i] = key_queue.keys[i + 1];
}

/* Read a key (DOS 07h) onto the queue; 0 if it was an extended key that
   is not a cursor key (it is dropped). */
static
queue_key()
{
	unsigned n = key_queue.count;
	unsigned char c = dos_getch();

	key_queue.keys[n] = c;
	++key_queue.count;
	if (c == 0) {
		switch (dos_getch()) {
		case 75: key_queue.keys[n] = 8; break;
		case 72: key_queue.keys[n] = 9; break;
		case 77: key_queue.keys[n] = 10; break;
		case 80: key_queue.keys[n] = 11; break;
		default:
			key_queue.keys[n] = 0;
			--key_queue.count;
			return 0;
		}
	} else if (c >= 'a' && c <= 'z') {
		/* a word-sized and: the byte after the key is cleared too */
		key_queue.keys[n] &= 0xdf;
		key_queue.keys[n + 1] = 0;
	}
	return 1;
}

get_filtered_key(mode)
{
	unsigned char c;

	for (;;) {
		if (key_queue.count) {
			if (passes(mode, key_queue.keys[0])) {
				--key_queue.count;
				return key_queue.keys[0];
			}
			drop_key();
			continue;
		}
		wait_for_music(0);	/* the PLAY driver's hook */
		if (mode == 0)
			return dos_getch();
		queue_key();
	}
}

poll_filtered_key(mode)
{
	stir_random_seeds();
	for (;;) {
		if (key_queue.count) {
			if (passes(mode, key_queue.keys[0]))
				return 1;
			drop_key();
			continue;
		}
		if (bios_key_peek() < 0)
			return 0;
		if (mode == 0)
			return 1;
		queue_key();
	}
}
