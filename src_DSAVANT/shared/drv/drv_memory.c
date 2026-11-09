/* The driver's memory: blocks in the maze area and in EMS, the slot tables
   that name them, releasing and compacting, and the free-space query.  On
   DOS: free_*.s, decrement_*.s, renumber_and_release_block.s,
   release_maze_block.s, release_and_compact_ems_blocks.s, ems_map_pages.s,
   map_compaction_pages.s, copy_file_region.s, get_free_space.s. */
#include "drv.h"
#include "dosfile.h"
#include "hostlib.h"

/* A new block at the end of the maze area, numbered in *slot; returns its
   entry, the paragraph offset filled in and the size left to the caller. */
unsigned *
new_maze_block(slot)
unsigned *slot;
{
	unsigned *entry;

	*slot = ++maze_block_count;
	entry = &maze_blocks[(maze_block_count - 1) * 2];
	entry[0] = maze_paras_used;
	return entry;
}

/* The same in EMS. */
unsigned *
new_ems_block(slot)
unsigned *slot;
{
	unsigned *entry;

	*slot = ++ems_block_count;
	entry = &ems_blocks[(ems_block_count - 1) * 2];
	entry[0] = ems_paras_used;
	return entry;
}

/* The entry of block id (numbered from 1). */
unsigned *
maze_block(id)
unsigned id;
{
	return &maze_blocks[(id - 1) * 2];
}

unsigned *
ems_block(id)
unsigned id;
{
	return &ems_blocks[(id - 1) * 2];
}

/* Every block number above id in a table of n entries, step words apart,
   moves down one. */
static
renumber(table, n, step, id)
unsigned *table, id;
{
	for (; n; n--, table += step)
		if (id < *table)
			--*table;
}

static
renumber_scores(id)
unsigned id;
{
	unsigned *p;
	int n;

	for (p = score_slots, n = 5; n; n--, p += 2)
		if (id < *p) {
			--*p;
			dos_write(1, "\r\n\007Score slot moved.\n", 21);
		}
}

/* Removes maze block id: the blocks after it move down over it, and the
   slot tables follow. */
renumber_and_release_block(id)
unsigned id;
{
	renumber(maze_map_slots, 10, 1, id);
	renumber(picture_slots, 20, 1, id);
	renumber(screen_maze_slots, 5, 1, id);
	renumber(maze_slots, 16, 1, id);
	renumber(sound_slots, 20, 2, id);
	renumber_scores(id);
	release_maze_block(id);
}

release_maze_block(id)
unsigned id;
{
	unsigned *entry, released;

	entry = maze_block(id);
	released = entry[1];
	maze_paras_used -= released;
	while (entry[3]) {
		host_memmove(MAZE_PTR(entry[0], 0), MAZE_PTR(entry[2], 0),
			(long)entry[3] << 4);
		entry[0] = entry[2] - released;
		entry[2] = entry[0] + entry[3];
		entry[1] = entry[3];
		entry += 2;
	}
	entry[0] = entry[1] = 0;
}

/* The EMS cache's tables follow a released EMS block. */
decrement_cached_slot_ids(id)
unsigned id;
{
	renumber(ems_slots, 20, 1, id);
	renumber(cache_maze_slot_table, 10, 1, id);
	renumber(screen_ems_slots, 10, 1, id);
}

release_and_compact_ems_blocks(id)
unsigned id;
{
	unsigned *entry, released;

	entry = ems_block(id);
	released = entry[1];
	ems_paras_used -= released;
	while (entry[3]) {
		host_memmove(EMS_PTR(entry[0]), EMS_PTR(entry[2]), (long)entry[3] << 4);
		entry[0] = entry[2] - released;
		entry[2] = entry[0] + entry[3];
		entry[1] = entry[3];
		entry += 2;
	}
	entry[0] = entry[1] = 0;
}

/* Frees the maze block a slot holds. */
static
free_slot(slot)
unsigned *slot;
{
	unsigned id;

	if ((id = *slot) != 0) {
		*slot = 0;
		maze_block_count--;
		renumber_and_release_block(id);
	}
}

free_picture(n)
unsigned n;
{
	if (n < 20)
		free_slot(&picture_slots[n]);
}

free_maze(n)
unsigned n;
{
	if (n < 10)
		free_slot(&maze_map_slots[n]);
}

/* Screens saved in the maze area are numbered 0, -1, ... -4. */
free_screen(n)
unsigned n;
{
	n = -n;
	if (n < 5)
		free_slot(&screen_maze_slots[n]);
}

free_sound(n)
unsigned n;
{
	if (n < 20)
		free_slot(&sound_slots[n * 2]);
}

free_score(n)
unsigned n;
{
	if (n < 5)
		free_slot(&score_slots[n * 2]);
}

/* Copies the rest of file in to file out, 4 KB at a time. */
copy_file_region(in, out)
{
	unsigned char *buf;
	long n;

	buf = DOS_PTR(scratch_buf_seg, 0);
	while ((n = dos_read(in, buf, 0x1000)) > 0)
		if (dos_write(out, buf, (unsigned)n) != n)
			dos_fatal("\r\n\007Error occurred writing to disk\n$");
}

/* Bytes free: 0 the largest DOS block, 1 EMS, 3 the disk (drive is DOS's
   number, 0 the current one), anything else the maze area. */
long
get_free_space(kind, drive)
{
	unsigned largest;

	if (kind == 0) {
		largest = 0;
		dos_alloc(0xffff, &largest);
		return (long)largest << 4;
	}
	if (kind == 1)
		return (long)(unsigned)(ems_para_limit - ems_paras_used) << 4;
	if (kind == 3)
		return dos_disk_free();
	return (long)(unsigned)(MAZE_MEM_PARAS - maze_paras_used) << 4;
}
