#include <stdlib.h>
#include <string.h>
#include "hostlib.h"

void *host_calloc(size_t size) { return calloc(1, size ? size : 1); }
void host_free(void *p) { free(p); }
void *host_realloc(void *p, size_t size) { return realloc(p, size); }
void host_memmove(void *dst, const void *src, size_t n) { memmove(dst, src, n); }
void host_memset(void *dst, int c, size_t n) { memset(dst, c, n); }
