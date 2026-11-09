/* :ts=8 */
/* Copyright (C) 1983 by Manx Software Systems */
/* Host version: the break moves through a fixed area (aztec_Croot takes
   its argv from it; malloc no longer uses it).  Returns the old break, or
   (char *)-1 with errno set when the area is used up. */
extern int errno;

static char area[4096];
static unsigned brk_used;

char *
aztec_sbrk(siz)
{
	char *old = area + brk_used;

	if (siz < 0 ? (unsigned)-siz > brk_used : siz > sizeof area - brk_used) {
		errno = -8;
		return (char *)-1;
	}
	brk_used += siz;
	return old;
}
