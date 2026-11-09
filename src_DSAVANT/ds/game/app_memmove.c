/* copies n bytes between far addresses */
#include "hostlib.h"

app_memmove(src, dst, n)
char *src;
char *dst;
unsigned n;
{
	host_memmove(dst, src, n);
}
