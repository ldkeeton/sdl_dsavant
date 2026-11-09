/* Fonts: each is loaded into a DOS block of its own, recorded in
   font_seg_table[slot].  The font starts with a 16-byte header
     +0  glyph width     +1  glyph height
     +2  nonzero: no extra block      +5  bytes per glyph row unit
     +6  flags: bit 1 an extra block, bit 2 it is in the file too
     +7  signed spacing  +0Ah glyph count / glyph record size
     +0Ch where the extra block goes  +0Eh extra glyph count
   then a 256-word table (+10h, low byte the record number, high byte the
   width) and the glyph records from +110h. */
#include "dosfile.h"
#include "dosmem.h"
#include "dshw.h"

#define WORD(p, o)	((p)[o] | (p)[(o) + 1] << 8)

load_font_file(handle, slot)
{
	unsigned char *hdr = DOS_PTR(font_header_seg, 0), *font;
	unsigned glyph_bytes, font_size, alloc_size, extra, seg, i, dest;
	long r;

	if (dos_read(handle, hdr, 16) < 0)
		dos_fatal("\007\r\nI/O error loading font\r\n$");
	glyph_bytes = (unsigned)(WORD(hdr, 0x0a) * hdr[5]);
	font_size = alloc_size = glyph_bytes + 0x110;
	if (hdr[2] == 0 && (hdr[6] & 2)) {
		extra = (unsigned)(WORD(hdr, 0x0e) * hdr[5]);
		alloc_size += extra;
		if (hdr[6] & 4)
			font_size += extra;
	}
	r = dos_alloc((unsigned)(alloc_size + 15) >> 4, 0);
	if (r < 0)
		font_memory_error();
	seg = r;
	font_seg_table[slot] = seg;
	font = DOS_PTR(seg, 0);
	for (i = 0; i < 16; i++)
		font[i] = hdr[i];
	if (dos_read(handle, font + 0x10, font_size - 0x10) < 0)
		dos_fatal("\007\r\nI/O error loading font\r\n$");
	/* an extra block that is not in the file starts as a copy of the
	   glyphs (byte by byte, as rep movsb) */
	if ((font[6] & 6) == 2 && font[2] == 0) {
		dest = WORD(font, 0x0c);
		for (i = 0; i < glyph_bytes; i++)
			*DOS_PTR(seg, dest + i) = *DOS_PTR(seg, 0x110 + i);
	}
}

/* The glyph record of a glyph: the table entry's low byte times the
   record size, past the header and table (16-bit, as on DOS).  The glyph
   index is doubled in a byte, so glyphs 128 and up wrap around. */
static unsigned
glyph_offset(font, glyph)
unsigned char *font; unsigned glyph;
{
	unsigned char index = font[0x10 + (unsigned char)(glyph << 1)];

	return (unsigned)(index * WORD(font, 0x0a) + 0x110);
}

/* A host pointer to the glyph's record; the width and height of the font
   into *glyph_w and *glyph_h. */
unsigned char *
get_font_glyph(font, glyph, glyph_w, glyph_h)
unsigned *glyph_w, *glyph_h;
{
	unsigned seg = font_seg_table[font];
	unsigned char *p = DOS_PTR(seg, 0);

	*glyph_w = p[0];
	*glyph_h = p[1];
	return DOS_PTR(seg, glyph_offset(p, glyph & 0xff));
}

/* Reads record `source` of the file into the glyph's record. */
read_font_glyph(handle, font, source, glyph)
{
	unsigned seg = font_seg_table[font];
	unsigned char *p = DOS_PTR(seg, 0);
	unsigned stride = WORD(p, 0x0a);

	dos_lseek(handle, (long)(source & 0xff) * stride, 0);
	dos_read(handle, DOS_PTR(seg, glyph_offset(p, glyph & 0xff)), stride);
}

/* Draws the glyph at the text position (the driver's draw_glyph). */
draw_font_glyph(glyph, font)
{
	unsigned seg = font_seg_table[font];

	if (seg)
		draw_glyph(seg, glyph);
}

draw_text_glyphs(text, font)
unsigned char *text;
{
	while (*text)
		draw_font_glyph(*text++, font);
}

/* The sum of the characters' widths plus the font's spacing for each; -1
   when the font is not loaded. */
measure_text_width(text, font)
unsigned char *text;
{
	unsigned seg = font_seg_table[font];
	unsigned char *p;
	int width, spacing;

	if (seg == 0)
		return -1;
	p = DOS_PTR(seg, 0);
	spacing = (signed char)p[7];
	for (width = 0; *text; text++)
		width += p[0x10 + *text * 2 + 1] + spacing;
	return width;
}

/* Frees every font block (the table's unused zeros included, which DOS
   refuses). */
free_font_memory()
{
	unsigned *s;

	for (s = font_seg_table; *s != 0xffff; s++)
		dos_free(*s);
}
