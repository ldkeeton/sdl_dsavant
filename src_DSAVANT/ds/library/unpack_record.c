/* unpack_record(record, output): the record's first byte is how many
   characters follow, Huffman-coded MSB first with the tree in the misc
   table.  A tree node is two words, the 0 and 1 edges: a non-negative edge
   is a leaf (its low byte the character), a negative one -n is node n.
   The output ends with a zero word. */
#include "dosmem.h"
#include "dshw.h"

unpack_record(record_buffer, output_buffer)
unsigned char *record_buffer, *output_buffer;
{
	unsigned char *src = record_buffer, *dst = output_buffer;
	unsigned length, bits, node, edge;
	unsigned char byte;

	length = *src++;
	bits = 8;
	byte = *src++;
	while (length) {
		/* decode_packed_tree_leaf */
		node = 0;
		for (;;) {
			int bit = byte >> 7;

			byte <<= 1;
			edge = *DOS_PTR(misc_table_seg, node + bit * 2) |
				*DOS_PTR(misc_table_seg, node + bit * 2 + 1) << 8;
			if (!(edge & 0x8000))
				break;
			node = (unsigned)((0x10000L - edge) << 2);
			if (--bits == 0) {
				bits = 8;
				byte = *src++;
			}
		}
		if (--bits == 0) {
			bits = 8;
			byte = *src++;
		}
		*dst++ = edge;
		--length;
	}
	dst[0] = 0;
	dst[1] = 0;
}
