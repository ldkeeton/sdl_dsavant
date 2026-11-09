/* Reports each stubbed function the first time it is called. */
#include <stdio.h>
#include <string.h>
#include "stubs.h"

long host_stub(const char *name)
{
    static const char *seen[1024];
    static int n;
    for (int i = 0; i < n; i++)
        if (seen[i] == name)
            return 0;
    if (n < 1024)
        seen[n++] = name;
    fprintf(stderr, "stub: %s\n", name);
    return 0;
}
