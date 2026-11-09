/* draw_text: draws frames of a loaded picture into the screen buffer,
   clipped, mirrored, shrunk by a percentage (dropping evenly spread
   columns and rows) and brightened.  It drew the game's sprites and
   portraits; the name is the driver's.  On DOS: draw_text.s,
   render_text_glyph_by_mode.s, set_text_bitmap_bits.s,
   check_frame_index.s.

   A picture block starts with its frame count, then a {offset, paragraph}
   pair a frame.  A frame starts with the offset of its pixels (from the
   frame), its width and height in 8x8 cells, then a bit a cell (LSB
   first): whether the cell is stored.  Stored cells are 64 bytes, row by
   row; 0FFh is transparent.

   Dropped columns and rows are bits (MSB first) in text_state_words and
   text_state_bytes; clipping drops more of them, so the renderer has only
   the masks to follow.  Offsets into the screen buffer are 16-bit. */
#include "drv.h"
#include "dosfile.h"

extern drv_fatal_number();

#define WORD(p)	(*(p) | (p)[1] << 8)
#define SCREEN(off)	(*DOS_PTR(screen_buf_seg, off))

/* What draw_text works out for the renderer (its stack frame, on DOS). */
static unsigned frame_seg, frame_off;	/* the frame being drawn */
static unsigned cols, rows;		/* of the first frame, in cells */
static unsigned dest;			/* screen offset of the frame's corner */
static unsigned left_skip, right_skip, top_skip, bottom_skip;
static unsigned width_m1;		/* cols * 8 - 1 */
static unsigned bottom_limit, top_limit;	/* (clip bottom + 1) * 320, clip top * 320 */
static unsigned char adjust;		/* brightening, percent / 12 */
static unsigned char draw_mode;		/* 0, 1 mirrored, 2 flipped, 3 both */
static unsigned char add_mode;		/* 2: frames add to the screen */

/* Spreads n set bits evenly over limit bits of table. */
static
set_text_bitmap_bits(table, limit, n)
unsigned char *table;
unsigned limit, n;
{
	unsigned stride, rem, acc, bit;

	stride = limit / n;
	rem = limit % n;
	acc = n >> 1;
	bit = stride - 1;
	for (;;) {
		acc += rem;
		if (acc >= n) {
			acc -= n;
			bit++;
		}
		if (bit >= limit || (bit & 0x8000))
			return;
		table[bit >> 3] |= 0x80 >> (bit & 7);
		bit += stride;
	}
}

/* A cursor over a mask table. */
static unsigned char *mask_byte, mask_bit;

#define MASKED()	(*mask_byte & mask_bit)
#define NEXT_BIT()	((mask_bit = mask_bit >> 1 | mask_bit << 7) == 0x80 ? mask_byte++ : 0)

/* Drops the next n bits that are not dropped yet; returns how many
   already-dropped ones it passed. */
static unsigned
drop_bits(n)
unsigned n;
{
	unsigned passed;

	passed = 0;
	while (n) {
		if (MASKED())
			passed++;
		else {
			*mask_byte |= mask_bit;
			n--;
		}
		NEXT_BIT();
	}
	return passed;
}

/* Column masks for clipping: skip columns off one side; then, past the
   visible ones, the rest.  Returns 0 if nothing is left to draw. */
static
clip_columns(skip, width, visible)
unsigned skip, width, visible;
{
	unsigned before, extra, count, n;

	mask_byte = text_state_words;
	mask_bit = 0x80;
	before = drop_bits(skip);
	if (width <= before + skip)
		return 0;
	extra = width - (before + skip);
	if (extra <= visible)
		return 1;
	extra -= visible;
	count = 0;
	n = visible;
	do {
		if (MASKED()) {
			if (++count == extra)
				break;
			n++;
		}
		NEXT_BIT();
	} while (--n);
	n = before + skip + visible + count;
	if (width <= n)
		return 1;
	for (n = width - n; n; n--) {
		*mask_byte |= mask_bit;
		NEXT_BIT();
	}
	return 1;
}

/* The frame's entry: exits if frame (from 1) is past the picture's
   count. */
static
find_frame(pic_seg, frame, pic)
unsigned pic_seg, frame, pic;
{
	unsigned char *entry;

	frame--;
	if (frame >= WORD(DOS_PTR(pic_seg, 0)))
		drv_fatal_number("\r\n\007Frame ## too big in slot ##.\n", frame + 1, pic);
	entry = DOS_PTR(pic_seg, 2 + (unsigned char)(frame << 2));
	frame_off = WORD(entry);
	frame_seg = pic_seg + WORD(entry + 2);
}

static
put(di, c)
unsigned di, c;
{
	unsigned lim, sum;

	if (add_mode == 2) {
		lim = intensity_limits[SCREEN(di)];
		sum = SCREEN(di) + c;
		c = sum > 255 || (sum & 255) >= lim ? lim : sum;
	} else if (adjust) {
		lim = intensity_limits[c];
		sum = c + adjust;
		c = sum > 255 || sum >= lim ? lim : sum;
	}
	SCREEN(di) = c;
}

static unsigned
bits_set(b)
unsigned b;
{
	unsigned n;

	for (n = 0; b; b >>= 1)
		n += b & 1;
	return n;
}

/* Draws the current frame with the masks draw_text set up. */
static
render_frame()
{
	unsigned start, di, si, cell_bits, row_adj, col_adj, cy, cx, k, j, d, d2, s, xstep, ystep;
	unsigned char *row_mask, *col_mask, cmask, rbit, pbit, c;
	int mirror, flip;

	mirror = draw_mode == 1 || draw_mode == 3;
	flip = draw_mode == 2 || draw_mode == 3;
	xstep = mirror ? -1 : 1;
	ystep = flip ? -320 : 320;
	if (!flip)
		start = dest + top_skip * 320;
	else
		start = rows * 0xa00 - 320 + dest - bottom_skip * 320;
	if (!mirror)
		start += left_skip;
	else
		start -= right_skip;
	si = WORD(DOS_PTR(frame_seg, frame_off)) + frame_off;
	cell_bits = frame_off + 4;
	cmask = 1;
	row_mask = text_state_bytes;
	cy = rows;
	do {
		di = mirror ? start + width_m1 : start;
		if (!flip ? di >= bottom_limit : !mirror ? di < top_limit : di <= top_limit)
			return;
		row_adj = bits_set(*row_mask) * 320;
		col_mask = text_state_words;
		cx = cols;
		do {
			col_adj = bits_set(*col_mask);
			if (*DOS_PTR(frame_seg, cell_bits) & cmask) {
				if (*col_mask == 0xff)
					si += 64;
				else {
					d = di;
					s = si;
					rbit = 0x80;
					k = 8;
					do {
						if (flip ? d < top_limit : d >= bottom_limit)
							break;
						if (*row_mask & rbit)
							s += 8;
						else {
							d2 = d;
							pbit = 0x80;
							j = 8;
							do {
								if (!(*col_mask & pbit)) {
									c = *DOS_PTR(frame_seg, s);
									if (c != 0xff)
										put(d2, c);
									d2 += xstep;
								}
								s++;
								pbit = pbit >> 1 | pbit << 7;
							} while (--j);
							d += ystep;
						}
						rbit = rbit >> 1 | rbit << 7;
					} while (--k);
					si += 64;
				}
			}
			if (!mirror)
				di += 8 - col_adj;
			else
				di -= 8 - col_adj;
			cmask = cmask << 1 | cmask >> 7;
			if (cmask == 1)
				cell_bits++;
			col_mask++;
		} while (--cx);
		if (!flip)
			start += 0xa00 - row_adj;
		else
			start -= 0xa00 - row_adj;
		row_mask++;
	} while (--cy);
}

/* Draws the frames listed in frames (ending in 0) of picture slot pic at
   (x, y), clipped to (clip_x1, clip_y1)-(clip_x2, clip_y2) inclusive.
   mode: bit 0 mirrors, bit 1 flips; add 2 adds the frames to the screen;
   percent > 0 shrinks them by that much (and brightens them). */
draw_text(pic, x, y, clip_x1, clip_x2, clip_y1, clip_y2, mode, frames, add, percent)
unsigned pic, x, y, clip_x1, clip_x2, clip_y1, clip_y2, mode, add, percent;
char *frames;
{
	unsigned pic_seg, w8, h8, n, visible, end;

	if (clip_x2 < clip_x1 || clip_y2 < clip_y1)
		return;
	dest = y * 320 + x;
	pic_seg = maze_mem_seg + maze_blocks[(picture_slots[pic] - 1) * 2];
	if (!frames[0])
		return;
	find_frame(pic_seg, (unsigned char)frames[0], pic);
	cols = DOS_PTR(frame_seg, frame_off)[2];
	rows = DOS_PTR(frame_seg, frame_off)[3];
	if (!(cols | rows))
		return;
	for (n = 0; n < 40 + 25; n++)
		text_state[n] = 0;
	w8 = cols << 3;
	h8 = rows << 3;
	if (percent) {
		if (percent >= 100)
			return;
		n = (long)w8 * percent / 100;
		if ((long)w8 * percent % 100 >= 50)
			n++;
		if (n)
			set_text_bitmap_bits(text_state_words, w8, n);
		n = (long)h8 * percent / 100;
		if ((long)h8 * percent % 100 >= 50)
			n++;
		if (n)
			set_text_bitmap_bits(text_state_bytes, h8, n);
	}

	left_skip = (int)clip_x1 >= (int)x ? clip_x1 - x : 0;
	width_m1 = w8 - 1;
	end = x + width_m1 + 1;
	right_skip = (int)end >= (int)clip_x2 ? end - clip_x2 : 0;
	n = right_skip + left_skip;
	if (w8 <= n)
		return;
	visible = w8 - n;
	top_skip = (int)clip_y1 >= (int)y ? clip_y1 - y : 0;
	end = y + h8 - 1;
	bottom_skip = (int)end >= (int)clip_y2 ? end - clip_y2 : 0;
	n = bottom_skip + top_skip;
	if (h8 <= n)
		return;
	bottom_limit = (clip_y2 + 1) * 320;
	top_limit = clip_y1 * 320;

	if (!clip_columns(mode & 1 ? right_skip : left_skip, w8, visible))
		return;
	mask_byte = text_state_bytes;
	mask_bit = 0x80;
	if (!(mode & 2)) {
		n = drop_bits(top_skip);
		if (h8 <= n + top_skip)
			return;
	} else {
		n = drop_bits(bottom_skip);
		if (h8 <= n + bottom_skip)
			return;
	}

	adjust = percent / 12;
	draw_mode = mode;
	add_mode = add;
	for (; *frames; frames++) {
		find_frame(pic_seg, (unsigned char)*frames, pic);
		render_frame();
	}
}
