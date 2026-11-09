/* allocates a far block (host: from the C heap, zeroed) */
#include "hostlib.h"

char *
app_malloc(size)
long size;
{
	return host_calloc(size);
}
