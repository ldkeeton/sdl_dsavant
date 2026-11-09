/* Copyright (C) 1985 by Manx Software Systems */
/* Host version: the int 21h functions aztec_open issues, 3Ch (create) and
   3Dh (open, with the mode in AL); a function number below 256 is in AH.
   Others fail with EINVAL. */
#include "errno.h"
#include "dosfile.h"

extern int errno;

aztec_impl_sys(func, arg, arg2)
unsigned func; char *arg;
{
	long r;

	if ((func >> 8) == 0)
		func <<= 8;
	switch (func >> 8) {
	case 0x3c:
		r = dos_create(arg, arg2);
		break;
	case 0x3d:
		r = dos_open(arg, func & 0xff);
		break;
	default:
		r = -EINVAL;
		break;
	}
	if (r < 0) {
		errno = -r;
		return -1;
	}
	return r;
}
