/* Copyright (C) 1983 by Manx Software Systems */
/* setmem(dest, len, val) */
#include "hostlib.h"

aztec_setmem(src, len, val)
char *src; unsigned len;
{
	host_memset(src, val & 0xff, len);
}
