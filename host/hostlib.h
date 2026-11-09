/* C library calls for game C.  Game C cannot include the system headers:
   its include path has Aztec's stdio.h, errno.h, ..., which the system
   headers would pick up in place of their own.  So the host wraps what game
   C needs here (hostlib.c, compiled natively). */
#ifndef HOSTLIB_H
#define HOSTLIB_H

#include <stddef.h>

void *host_calloc(size_t size);
void host_free(void *p);
void *host_realloc(void *p, size_t size);
void host_memmove(void *dst, const void *src, size_t n);
void host_memset(void *dst, int c, size_t n);

#endif
