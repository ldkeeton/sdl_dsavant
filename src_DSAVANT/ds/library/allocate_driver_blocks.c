/* Allocates each block of driver_block_table (when there is a video
   driver: result nonzero).  If one fails, the ones before it are freed and
   the program ends. */
#include "dosmem.h"
#include "dshw.h"

allocate_driver_blocks(result)
{
	unsigned *entry;
	long seg;

	if (result == 0)
		return;
	for (entry = &driver_block_table[DRV_BLOCK_FIRST]; entry[0]; entry += 2) {
		seg = dos_alloc(entry[0], 0);
		if (seg < 0) {
			free_driver_blocks(entry);
			dos_fatal("\r\n\007Unable to allocate sufficient memory to run.\r\n$");
		}
		entry[1] = seg;
	}
}

/* Frees the blocks of the entries before `end`, back to the zero entry. */
free_driver_blocks(end)
unsigned *end;
{
	while (end -= 2, end[0])
		dos_free(end[1]);
}

/* On exit.  The original started its walk two bytes short of the table's
   end, so it read each segment as a size and the next size as a segment,
   and DOS refused every free; it is kept that way (the process is ending
   and the memory goes with it). */
free_driver_and_font_memory()
{
	free_driver_blocks((unsigned *)((char *)&driver_block_table[DRV_BLOCK_END] - 2));
	free_font_memory();
}
