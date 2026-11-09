/* Copyright (C) 1985 by Manx Software Systems, Inc. */
/* Host version: the heap is the host's.  Aztec's own allocator kept its
   free list in 16-bit words, which cannot hold a host pointer.  Blocks
   come back zeroed (the game does not rely on what was in them). */
#include "hostlib.h"

char *
aztec_malloc(size)
unsigned size;
{
	return host_calloc(size);
}

char *
aztec_realloc(area, size)
char *area; unsigned size;
{
	return host_realloc(area, size ? size : 1);
}

aztec_free(p)
char *p;
{
	host_free(p);
	return 0;
}
