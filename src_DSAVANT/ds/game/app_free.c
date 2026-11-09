/* frees a far block from app_malloc */
#include "hostlib.h"

app_free(p)
char *p;
{
	host_free(p);
}
