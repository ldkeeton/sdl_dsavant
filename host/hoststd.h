/* <stddef.h> and <stdint.h> for the host headers game C includes.
   mingw's <stddef.h> defines errno as a macro for the C library's own;
   in game C (host16.h is in force) errno is Aztec's, so it is put back. */
#ifndef HOSTSTD_H
#define HOSTSTD_H

#include <stddef.h>
#include <stdint.h>

#ifdef HOST16_H
#undef errno
#define errno aztec_errno
#endif

#endif
