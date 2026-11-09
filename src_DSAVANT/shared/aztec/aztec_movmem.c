/* Copyright (C) 1984, 1985 by Manx Software Systems */
/* movmem(src, dest, len): overlapping moves are safe. */
#include "hostlib.h"

aztec_movmem(arg1, arg2, len)
char *arg1, *arg2; unsigned len;
{
	host_memmove(arg2, arg1, len);
}
