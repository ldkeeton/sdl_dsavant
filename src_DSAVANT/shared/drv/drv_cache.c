/* Saving parts of the screen buffer and putting them back: save_area into
   a DOS block, cache_area into the maze area, EMS or a file, and the EMS
   caches of maze files and screens.  On DOS: save_area.s, restore_area.s,
   cache_area.s, restore_screen.s, cache_maze.s,
   cache_screen_to_storage.s.

   A saved area starts with a 12-byte header: {screen offset, 0, rows,
   columns, size low, size high}; a file one with 10 bytes: {size high,
   size low, columns, rows, screen offset}. */
#include "drv.h"
#include "dosfile.h"
#include "hostlib.h"

extern unsigned *new_maze_block(), *new_ems_block(), *maze_block(), *ems_block();

#define WORD(p)	(*(p) | (p)[1] << 8)
#define PUT_WORD(p, v)	((p)[0] = (v), (p)[1] = (unsigned)(v) >> 8)

/* Rows of the screen buffer from offset off into dst, and back. */
static
rows_from_screen(dst, off, rows, cols)
unsigned char *dst;
unsigned off, rows, cols;
{
	do {
		host_memmove(dst, DOS_PTR(screen_buf_seg, off), cols);
		dst += cols;
		off += 320;
	} while (--rows);
}

static
rows_to_screen(src, off, rows, cols)
unsigned char *src;
unsigned off, rows, cols;
{
	do {
		host_memmove(DOS_PTR(screen_buf_seg, off), src, cols);
		src += cols;
		off += 320;
	} while (--rows);
}

/* The 12-byte header and the rows of (x1, y1)-(x2, y2) at dst. */
static
save_rows(dst, x1, y1, rows, cols, size)
unsigned char *dst;
unsigned x1, y1, rows, cols;
long size;
{
	unsigned off;

	off = y1 * 320 + x1;
	PUT_WORD(dst, off);
	PUT_WORD(dst + 2, 0);
	PUT_WORD(dst + 4, rows);
	PUT_WORD(dst + 6, cols);
	PUT_WORD(dst + 8, (unsigned)size);
	PUT_WORD(dst + 10, (unsigned)(size >> 16));
	rows_from_screen(dst + 12, off, rows, cols);
}

/* Puts back what save_rows saved. */
static
restore_rows(src)
unsigned char *src;
{
	rows_to_screen(src + 12, WORD(src), WORD(src + 4), WORD(src + 6));
}

/* Saves (x1, y1)-(x2, y2), inclusive, in a new DOS block; returns it as a
   far pointer (segment:0). */
long
save_area(x1, y1, x2, y2)
unsigned x1, y1, x2, y2;
{
	unsigned cols, rows, paras;
	long size, seg;

	cols = x2 - x1 + 1;
	rows = y2 - y1 + 1;
	size = (long)cols * rows;
	paras = (unsigned)size + 12 + 15;
	paras >>= 4;
	seg = dos_alloc(paras, (unsigned *)0);
	if (seg < 0)
		dos_fatal("\r\n\007SAVEAREA out of memory.\n$");
	save_rows(DOS_PTR(seg, 0), x1, y1, rows, cols, size);
	return seg << 16;
}

/* Puts back an area save_area saved, freeing its block if release. */
restore_area(area, release)
long area;
{
	unsigned seg, off;

	seg = area >> 16;
	off = area;
	if ((seg | off) == 0)
		dos_fatal("\r\n\007RESTOREAREA called with NULL pointer.\n$");
	restore_rows(DOS_PTR(seg, off));
	if (release && seg)
		dos_free(seg);
}

static
disk_error()
{
	dos_fatal("\r\n\007Error occurred writing to disk in CACHEAREA().\n$");
}

/* Saves (x1, y1)-(x2, y2): mode 3 to file fd, 2 in the maze area, any
   other in EMS.  Returns mode << 16 | the slot, which restore_screen
   takes (a file has no slot: 0). */
long
cache_area(x1, y1, x2, y2, mode, fd)
unsigned x1, y1, x2, y2;
{
	unsigned cols, rows, paras, used, block, slot, off, *table, *entry;
	unsigned char header[10];
	long size;

	cols = x2 - x1 + 1;
	rows = y2 - y1 + 1;
	size = (long)cols * rows;
	if (mode == 3) {
		off = y1 * 320 + x1;
		PUT_WORD(header, (unsigned)(size >> 16));
		PUT_WORD(header + 2, (unsigned)size);
		PUT_WORD(header + 4, cols);
		PUT_WORD(header + 6, rows);
		PUT_WORD(header + 8, off);
		if (dos_write(fd, header, 10) < 0)
			disk_error();
		do {
			if (dos_write(fd, DOS_PTR(screen_buf_seg, off), cols) != cols)
				disk_error();
			off += 320;
		} while (--rows);
		return (long)mode << 16;
	}
	paras = (unsigned)size + 12 + 15;
	paras >>= 4;
	if (mode == 2) {
		used = paras + maze_paras_used;
		if (used > MAZE_MEM_PARAS)
			dos_fatal("\r\n\007CACHEAREA out of MAZE/PIC memory.\n$");
		block = maze_paras_used;
		maze_paras_used = used;
		table = maze_slots;
		for (slot = 0; slot < 16 && table[slot]; slot++)
			;
		if (slot == 16)
			dos_fatal("\r\n\007CACHEAREA out of MAZE slots.\n$");
		table[slot] = ++maze_block_count;
		entry = &maze_blocks[(maze_block_count - 1) * 2];
		entry[0] = block;
		entry[1] = paras;
		save_rows(MAZE_PTR(block, 0), x1, y1, rows, cols, size);
	} else {
		used = paras + ems_paras_used;
		if (used > ems_para_limit)
			dos_fatal("\r\n\007CACHEAREA out of EMS.\n$");
		block = ems_paras_used;
		ems_paras_used = used;
		table = ems_slots;
		for (slot = 0; slot < 20 && table[slot]; slot++)
			;
		if (slot == 20)
			dos_fatal("\r\n\007CACHEAREA out of EMS slots.\n$");
		table[slot] = ++ems_block_count;
		entry = &ems_blocks[(ems_block_count - 1) * 2];
		entry[0] = block;
		entry[1] = paras;
		save_rows(EMS_PTR(block), x1, y1, rows, cols, size);
	}
	return (long)mode << 16 | slot;
}

/* Puts back an area cache_area saved: slot and mode are its handle (mode
   3: read from file fd).  With release, its memory is freed. */
restore_screen(slot, mode, release, fd)
unsigned slot;
{
	unsigned char header[10];
	unsigned off, rows, cols, id;

	if (mode == 3) {
		dos_read(fd, header, 10);
		cols = WORD(header + 4);
		rows = WORD(header + 6);
		off = WORD(header + 8);
		do {
			dos_read(fd, DOS_PTR(screen_buf_seg, off), cols);
			off += 320;
		} while (--rows);
	} else if (mode == 2) {
		restore_rows(MAZE_PTR(maze_block(maze_slots[slot])[0], 0));
		if (release) {
			id = maze_slots[slot];
			maze_slots[slot] = 0;
			maze_block_count--;
			renumber_and_release_block(id);
		}
	} else {
		restore_rows(EMS_PTR(ems_block(ems_slots[slot])[0]));
		if (release) {
			ems_block_count--;
			id = ems_slots[slot];
			ems_slots[slot] = 0;
			decrement_cached_slot_ids(id);
			release_and_compact_ems_blocks(id);
		}
	}
}

/* Frees EMS slot *slot (release paths of cache_maze and
   cache_screen_to_storage). */
static
release_ems_slot(slot)
unsigned *slot;
{
	unsigned id;

	ems_block_count--;
	id = *slot;
	*slot = 0;
	decrement_cached_slot_ids(id);
	release_and_compact_ems_blocks(id);
}

/* Reads the rest of file fd into EMS at dst, 4 KB at a time through the
   scratch buffer. */
static
file_to_ems(fd, dst, left)
unsigned char *dst;
long left;
{
	unsigned char *scratch;
	unsigned n;

	scratch = DOS_PTR(scratch_buf_seg, 0);
	while (left) {
		n = left < 0x1000 ? left : 0x1000;
		left -= n;
		dos_read(fd, scratch, n);
		host_memmove(dst, scratch, n);
		dst += n;
	}
}

/* Caches maze file fd in EMS slot n (load_maze copies it from there).
   fd < 0 frees slot n instead; with copy, the file is copied to file n. */
cache_maze(fd, n, copy)
unsigned fd, n;
{
	unsigned *slot, *entry, block, last_size;
	unsigned char *scratch, *last;
	long total;

	if (fd & 0x8000) {
		release_ems_slot(&cache_maze_slot_table[n]);
		return;
	}
	if (copy) {
		copy_file_region(fd, n);
		return;
	}
	slot = &cache_maze_slot_table[n];
	if (*slot)
		dos_fatal("\r\n\007CACHEMAZE().  Requested slot already in use.\n$");
	entry = new_ems_block(slot);
	scratch = DOS_PTR(scratch_buf_seg, 0);
	dos_read(fd, scratch, 0x1000);
	last = scratch + (unsigned)(WORD(scratch) * 6 + 4 - 6);
	last_size = (unsigned)(last[4] * last[5]) << 3;
	last_size += WORD(last + 2);
	last_size += 15;
	total = ((long)WORD(last) << 4) + last_size;
	block = ems_paras_used;
	entry[1] = total / 16;
	ems_paras_used += entry[1];
	if (ems_paras_used > ems_para_limit)
		dos_fatal("\r\n\007CACHEMAZE().  Insufficient EMS.\n$");
	host_memmove(EMS_PTR(block), scratch, 0x1000L);
	file_to_ems(fd, EMS_PTR(block) + 0x1000, total - 0x1000);
}

/* Caches the 64000-byte screen file fd in EMS slot n (load_screen mode 2
   shows it).  fd < 0 frees slot n instead; with copy, the file is copied to
   file n. */
cache_screen_to_storage(fd, n, copy)
unsigned fd, n;
{
	unsigned *slot, *entry, block;

	if (fd & 0x8000) {
		release_ems_slot(&screen_ems_slots[n]);
		return;
	}
	if (copy) {
		copy_file_region(fd, n);
		return;
	}
	slot = &screen_ems_slots[n];
	if (*slot)
		dos_fatal("\r\n\007CACHESCREEN().  Requested slot already in use.\n$");
	entry = new_ems_block(slot);
	entry[1] = 0xfa0;
	block = ems_paras_used;
	ems_paras_used += 0xfa0;
	if (ems_paras_used > ems_para_limit)
		dos_fatal("\r\n\007CACHESCREEN().  Insufficient EMS.\n$");
	file_to_ems(fd, EMS_PTR(block), 64000L);
}
