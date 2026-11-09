/* Drawing into the screen buffer: pixels, filled rectangles, font glyphs
   at the text position, and maze pieces.  On DOS: put_pixel.s, fill_area.s,
   set_text_opaque.s, set_text_pos.s, draw_glyph.s, draw_maze_piece.s.

   Offsets into the buffer are 16-bit, as they were: the buffer is a 64 KB
   block, so a bad rectangle wraps inside it rather than running off. */
#include "drv.h"
#include "dosfile.h"

#define WORD(p)	(*(p) | (p)[1] << 8)
#define SCREEN(off)	(*DOS_PTR(screen_buf_seg, off))

put_pixel(x, y, color)
unsigned x, y;
{
	SCREEN(y * 320 + x) = color;
}

/* Fills (x1, y1)-(x2, y2), inclusive. */
fill_area(x1, y1, x2, y2, color)
unsigned x1, y1, x2, y2;
{
	unsigned off, rows, cols, n;

	off = y1 * 320 + x1;
	rows = y2 - y1 + 1;
	cols = x2 - x1 + 1;
	do {
		for (n = 0; n < cols; n++)
			SCREEN(off + n) = color;
		off += 320;
	} while (--rows);
}

set_text_opaque(on)
{
	text_opaque = on;
}

set_text_pos(x, y)
{
	text_pos_x = x;
	text_pos_y = y;
}

/* Draws glyph glyph of the font at segment font at the text position and
   advances it.  The font header: [0] cell width, [1] rows, [2] bit 0 a
   colour font (a byte a pixel), [3] bytes a row of a mono font, [6] bit 1
   masked or transparent, [7] extra spacing (signed), [0Ah] bytes per glyph
   unit, [0Ch], [0Eh] the mask table's offset and bytes per unit; at 10h a
   {unit, width} pair a glyph; the glyphs at 110h. */
draw_glyph(font, glyph)
unsigned font, glyph;
{
	unsigned char *hdr, *map, bits, mask, fg, bg;
	unsigned si, mi, di, row, width, cell, rows, skip, left, n;

	hdr = DOS_PTR(font, 0);
	di = text_pos_y * 320 + text_pos_x;
	row = di;
	cell = hdr[0];
	rows = hdr[1];
	map = hdr + 0x10 + (unsigned)(glyph << 1);
	si = map[0] * WORD(hdr + 0x0a) + 0x110;
	width = map[1];
	text_pos_x += width;
	text_pos_x += (signed char)hdr[7];
	fg = primary_text_color;
	bg = secondary_text_color;

	if (!(hdr[2] & 1) && (text_opaque || !(hdr[6] & 2))) {
		/* Mono: one bit a pixel, foreground or background. */
		skip = hdr[3] - ((width + 7) >> 3);
		do {
			left = width;
			do {
				bits = *DOS_PTR(font, si++);
				n = left < 9 ? left : 8;
				left -= n;
				do {
					SCREEN(di++) = bits & 0x80 ? fg : bg;
					bits <<= 1;
				} while (--n);
			} while (left);
			si += skip;
			row += 320;
			di = row;
		} while (--rows);
	} else if (!(hdr[2] & 1)) {
		/* Mono with a mask: pixels outside it are left alone. */
		mi = map[0] * WORD(hdr + 0x0e) + WORD(hdr + 0x0c);
		do {
			left = cell;
			do {
				bits = *DOS_PTR(font, si++);
				mask = *DOS_PTR(font, mi++);
				n = left < 9 ? left : 8;
				left -= n;
				do {
					if (mask & 0x80)
						SCREEN(di) = bits & 0x80 ? fg : bg;
					di++;
					mask <<= 1;
					bits <<= 1;
				} while (--n);
			} while (left);
			row += 320;
			di = row;
		} while (--rows);
	} else {
		/* Colour: copied, or with 0FFh transparent. */
		skip = cell - width;
		do {
			n = width;
			do {
				if (!(hdr[6] & 2) || *DOS_PTR(font, si) != 0xff)
					SCREEN(di) = *DOS_PTR(font, si);
				si++;
				di++;
			} while (--n);
			si += skip;
			row += 320;
			di = row;
		} while (--rows);
	}
}

/* Draws piece placement row of maze map slot n (its piece descriptors are
   5 bytes: piece, x (signed, cells), y, x offset and width in 8-pixel
   cells).  piece_index -1 draws it as stored; otherwise placement
   piece_index's position and width with row's piece, mirrored.  mode: 0
   copy, 2 with 0FFh transparent, 3 a black silhouette, others OR. */
draw_maze_piece(n, row, mode, piece_index)
unsigned n, row, piece_index;
{
	unsigned id, block, seg, src, di, cols, rows, stride, step, c;
	unsigned char *place, *piece;
	int mirror;

	id = maze_map_slots[n];
	if (!id)
		dos_fatal("\r\n\007MAZE SET ERROR.\n$");
	block = maze_blocks[(id - 1) * 2];
	seg = maze_mem_seg + block;
	place = DOS_PTR(seg, row * 5 + maze_piece_descriptors[n]);
	piece = DOS_PTR(seg, place[0] * 6 + maze_piece_table);
	mirror = piece_index != 0xffff;
	if (mirror)
		place = DOS_PTR(seg, piece_index * 5 + maze_piece_descriptors[n]);
	if (!place[4])
		return;
	cols = place[4] << 3;
	di = (signed char)place[1] + place[3];
	di <<= 3;
	di += place[2] * 320;
	stride = piece[4] << 3;
	if (!mirror)
		src = WORD(piece + 2) + (place[3] << 3);
	else
		src = WORD(piece + 2) + stride - 1 - (place[3] << 3);
	step = mirror ? -1 : 1;
	rows = piece[5];
	seg += WORD(piece);
	do {
		unsigned s, d, k;

		s = src;
		d = di;
		k = cols;
		do {
			c = *DOS_PTR(seg, s);
			s += step;
			if (mode == 0)
				SCREEN(d) = c;
			else if (mode == 2) {
				if (c != 0xff)
					SCREEN(d) = c;
			} else if (mode == 3) {
				if (c != 0xff)
					SCREEN(d) = 0;
			} else
				SCREEN(d) |= c;
			d++;
		} while (--k);
		di += 320;
		src += stride;
	} while (--rows);
}
