/* Loading into the maze area: pictures, mazes, whole screens, sounds and
   music scores.  On DOS: load_picture.s, load_maze.s, load_screen.s,
   save_screen.s, load_sound.s, expand_compressed_string.s,
   decode_compressed_string_byte.s, load_score.s. */
#include "drv.h"
#include "dosfile.h"
#include "hostlib.h"

extern unsigned *new_maze_block(), *maze_block(), *ems_block();

#define WORD(p)	(*(p) | (p)[1] << 8)

/* A fatal error naming two-digit numbers: msg has "##" where n1 goes, and
   where n2 goes if there is a second. */
drv_fatal_number(msg, n1, n2)
char *msg;
unsigned n1, n2;
{
	char buf[80];
	int i;

	for (i = 0; msg[i] && i < 78; i++) {
		buf[i] = msg[i];
		if (msg[i] == '#' && msg[i + 1] == '#') {
			buf[i] = '0' + n1 / 10 % 10;
			buf[++i] = '0' + n1 % 10;
			n1 = n2;
		}
	}
	buf[i] = '$';
	dos_fatal(buf);
}

/* The file is a 32-bit byte count, then the picture. */
load_picture(fd, n)
unsigned n;
{
	unsigned *slot, *entry, seg, paras, count;
	unsigned char *scratch;
	long size;

	slot = &picture_slots[n];
	if (*slot)
		drv_fatal_number("\r\n\007Picture slot ## not freed.\n", n, 0);
	entry = new_maze_block(slot);
	scratch = DOS_PTR(scratch_buf_seg, 0);
	dos_read(fd, scratch, 4);
	size = WORD(scratch) | (long)WORD(scratch + 2) << 16;
	paras = (size + 15) >> 4;
	maze_paras_used += paras;
	entry[1] = paras;
	if (maze_paras_used >= MAZE_MEM_PARAS)
		dos_fatal("\r\n\007Memory unavailable loading picture.\n$");
	for (seg = maze_mem_seg + entry[0]; size; seg += 0x100) {
		count = size < 0x1000 ? size : 0x1000;
		size -= count;
		dos_read(fd, DOS_PTR(seg, 0), count);
	}
}

/* Where a maze's piece placements start: after its piece table, a count
   then 6-byte entries. */
static unsigned
piece_descriptors(base)
unsigned char *base;
{
	return WORD(base) * 6 + 4;
}

/* Loads maze file fd into maze slot n; with from_ems, fd is instead a
   cache_maze slot to copy it from. */
load_maze(fd, n, from_ems)
unsigned fd, n;
{
	unsigned *slot, *entry, *cached, seg, desc, paras, last_size, chunks;
	unsigned char *base, *last;
	long total, rest;

	if (n >= 10)
		dos_fatal("\r\n\007LOAD_MAZE called with bad slot number.\n$");
	slot = &maze_map_slots[n];
	if (*slot)
		dos_fatal("\r\n\007LOAD_MAZE called with filled slot number.\n$");
	entry = new_maze_block(slot);
	if (from_ems) {
		cached = ems_block(cache_maze_slot_table[fd]);
		paras = cached[1];
		entry[1] = paras;
		seg = maze_mem_seg + maze_paras_used;
		maze_paras_used += paras;
		host_memmove(DOS_PTR(seg, 0), EMS_PTR(cached[0]), (long)paras << 4);
		maze_piece_descriptors[n] = piece_descriptors(DOS_PTR(seg, 0));
		return;
	}

	/* The first 4 KB holds the piece table; its last entry, {paragraph,
	   offset, width in 8-pixel cells, height}, ends the file. */
	seg = maze_mem_seg + maze_paras_used;
	base = DOS_PTR(seg, 0);
	dos_read(fd, base, 0x1000);
	desc = piece_descriptors(base);
	maze_piece_descriptors[n] = desc;
	last = base + (unsigned)(desc - 6);
	last_size = (unsigned)(last[4] * last[5]) << 3;
	last_size += WORD(last + 2);
	total = ((long)WORD(last) << 4) + last_size;
	rest = total - 0x1000;
	paras = (total + 15) / 16;
	entry[1] = paras;
	maze_paras_used += paras;
	if (maze_paras_used >= MAZE_MEM_PARAS)
		dos_fatal("\r\noops... LOAD_MAZE filled its memory area.\n$");
	seg += 0x100;
	for (chunks = rest >> 16; chunks; chunks--) {
		dos_read(fd, DOS_PTR(seg, 0), 0x8000);
		seg += 0x800;
		dos_read(fd, DOS_PTR(seg, 0), 0x8000);
		seg += 0x800;
	}
	dos_read(fd, DOS_PTR(seg, 0), (unsigned)rest);
}

/* A whole screen into the screen buffer: mode 2 from a cache_screen slot
   in EMS, 1 from a save_screen slot, anything else from file n. */
load_screen(n, mode)
unsigned n;
{
	unsigned *entry;

	if (mode == 2) {
		entry = ems_block(screen_ems_slots[n]);
		host_memmove(SCREEN_BUF, EMS_PTR(entry[0]), 64000L);
	} else if (mode == 1) {
		entry = maze_block(screen_maze_slots[n]);
		host_memmove(SCREEN_BUF, MAZE_PTR(entry[0], 0), 64000L);
	} else
		dos_read(n, SCREEN_BUF, 64000);
}

/* The screen buffer into maze slot -n (n is 0, -1 ... -4), or written to
   file n. */
save_screen(n)
unsigned n;
{
	unsigned *slot, *entry;

	if (!(n & 0x8000)) {
		dos_write(n, SCREEN_BUF, 64000);
		return;
	}
	slot = &screen_maze_slots[(unsigned)-n];
	if (*slot)
		dos_fatal("\r\n\007SAVE_SCREEN called with bad slot number.\n$");
	entry = new_maze_block(slot);
	entry[1] = 0xfa0;
	maze_paras_used += 0xfa0;
	if (maze_paras_used >= MAZE_MEM_PARAS)
		dos_fatal("\r\n\007SAVE_SCREEN called with table full.\n$");
	host_memmove(MAZE_PTR(entry[0], 0), SCREEN_BUF, 64000L);
}

/* A Huffman-coded sound: the tree is at scratch 0 (4-byte nodes of two
   children; a child with bit 15 set is node -child, otherwise a byte), the
   bit stream is read 4 KB at a time into scratch 400h..13FFh, MSB first. */
static unsigned char *huff_buf, huff_byte;
static unsigned huff_pos;
static int huff_bits, huff_fd;

static
huff_next()
{
	huff_bits = 8;
	huff_byte = huff_buf[huff_pos++];
	if (huff_pos >= 0x1400) {
		dos_read(huff_fd, huff_buf + 0x400, 0x1000);
		huff_pos = 0x400;
	}
}

static
huff_decode()
{
	unsigned node, child;
	int right;

	node = 0;
	for (;;) {
		right = huff_byte & 0x80;
		huff_byte <<= 1;
		child = WORD(huff_buf + node + (right ? 2 : 0));
		if (--huff_bits == 0)
			huff_next();
		if (!(child & 0x8000))
			return child & 0xff;
		node = -child;
		node <<= 2;
	}
}

static
expand_compressed_string(dst_seg, len)
unsigned dst_seg, len;
{
	unsigned di;

	huff_bits = 8;
	huff_byte = huff_buf[huff_pos++];
	for (di = 0; len; len--, di++)
		*DOS_PTR(dst_seg, di) = huff_decode();
}

/* The file starts with the size of its tree (0: not compressed); a
   compressed one's bit stream starts with its length. */
load_sound(fd, n)
unsigned n;
{
	unsigned *slot, *entry, tree, len, paras;
	unsigned char *scratch;

	slot = &sound_slots[n * 2];
	if (*slot)
		dos_fatal("\r\n\007In LOAD_SOUND() ... slot already used.\n$");
	entry = new_maze_block(slot);
	len = dos_lseek(fd, 0L, 2);
	dos_lseek(fd, 0L, 0);
	scratch = DOS_PTR(scratch_buf_seg, 0);
	dos_read(fd, scratch, 2);
	tree = WORD(scratch);
	if (tree == 0) {
		len -= 2;
		slot[1] = len;
		paras = len + 15;
		paras >>= 4;
		entry[1] = paras;
		paras += maze_paras_used;
		if (paras >= MAZE_MEM_PARAS)
			dos_fatal("\r\n\007In LOAD_SOUND() ... not enough memory.\n$");
		maze_paras_used = paras;
		dos_read(fd, MAZE_PTR(entry[0], 0), len);
		return 0;
	}
	dos_read(fd, scratch, tree);
	dos_read(fd, scratch + 0x400, 0x1000);
	len = WORD(scratch + 0x400);
	slot[1] = len;
	paras = len + 15;
	paras >>= 4;
	entry[1] = paras;
	paras += maze_paras_used;
	if (paras >= MAZE_MEM_PARAS)
		dos_fatal("\r\n\007In LOAD_SOUND() ... not enough memory.\n$");
	maze_paras_used = paras;
	huff_buf = scratch;
	huff_pos = 0x402;
	huff_fd = fd;
	expand_compressed_string(maze_mem_seg + entry[0], len);
	return 0;
}

/* A score into score slot n, after the patches in file patch_fd when that
   is not -1; info is kept for play_score. */
load_score(fd, patch_fd, info, n)
unsigned n;
{
	unsigned *slot, *entry, patch_len, patch_paras, len, paras, seg;

	slot = &score_slots[n * 2];
	if (*slot)
		dos_fatal("\r\n\007In LOAD_SCORE() ... slot already used.\n$");
	score_info[n] = info;
	entry = new_maze_block(slot);
	patch_len = patch_paras = 0;
	slot[1] = 0;
	if (patch_fd != -1) {
		patch_len = dos_lseek(patch_fd, 0L, 2);
		slot[1] = patch_len;
		patch_paras = patch_len + 15;
		patch_paras >>= 4;
		dos_lseek(patch_fd, 0L, 0);
	}
	len = dos_lseek(fd, 0L, 2);
	dos_lseek(fd, 0L, 0);
	paras = len + 15;
	paras >>= 4;
	paras += patch_paras;
	entry[1] = paras;
	paras += maze_paras_used;
	if (paras >= MAZE_MEM_PARAS)
		dos_fatal("\r\n\007In LOAD_SCORE() ... not enough memory.\n$");
	maze_paras_used = paras;
	seg = maze_mem_seg + entry[0];
	if (patch_fd != -1)
		dos_read(patch_fd, DOS_PTR(seg, 0), patch_len);
	dos_read(fd, DOS_PTR(seg, slot[1]), len);
	return 0;
}
