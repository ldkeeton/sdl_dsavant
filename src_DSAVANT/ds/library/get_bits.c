/* Bit helpers.  A shift count is taken mod 32 and a width of 0 gives an
   empty mask, as the 386 shifts did; a field is read or written as the
   16-bit little-endian word at its first byte. */

static unsigned
mask_of(width)
{
	return (unsigned)(0xffffUL >> ((16 - width) & 31));
}

/* (value >> shift), width bits of it */
get_bits(value, shift, width)
unsigned value;
{
	return (unsigned)((unsigned long)value >> (shift & 31)) & mask_of(width);
}

/* Field index of width bits in packed data. */
read_packed_field(packed_data, field_index, field_width)
unsigned char *packed_data; unsigned field_index;
{
	unsigned bit = (unsigned)(field_width * field_index);
	unsigned char *p = packed_data + (bit >> 3);
	unsigned word = p[0] | p[1] << 8;

	return (word >> (bit & 7)) & mask_of(field_width);
}

set_bitfield(packed_data, field_index, field_width, field_value)
unsigned char *packed_data; unsigned field_index; unsigned field_value;
{
	unsigned mask = mask_of(field_width);
	unsigned bit = (unsigned)(field_width * field_index);
	unsigned char *p = packed_data + (bit >> 3);
	unsigned word = p[0] | p[1] << 8;

	word = word & ~(mask << (bit & 7)) | (field_value & mask) << (bit & 7);
	p[0] = word;
	p[1] = word >> 8;
}

/* 1 if bit bit_index of the byte bitset is set. */
test_bit(bitset, bit_index)
unsigned char *bitset; unsigned bit_index;
{
	return (bitset[bit_index >> 3] & 1 << (bit_index & 7)) != 0;
}

set_bitmap_bit(bitmap, bit_index, value)
unsigned char *bitmap; unsigned bit_index;
{
	bitmap[bit_index >> 3] &= ~(1 << (bit_index & 7));
	if (value)
		bitmap[bit_index >> 3] |= 1 << (bit_index & 7);
}
