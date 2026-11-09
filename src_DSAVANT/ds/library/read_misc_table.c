/* Reads the 400h-byte misc table into its block; it holds the tree
   unpack_record decodes with. */
#include "dosfile.h"
#include "dosmem.h"
#include "dshw.h"

read_misc_table(handle)
{
	if (dos_read(handle, DOS_PTR(misc_table_seg, 0), 0x400) < 0)
		dos_fatal("\007\r\nI/O error loading Misc. table.\r\n$");
}
