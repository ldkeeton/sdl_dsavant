/* Copyright (C) 1983 by Manx Software Systems */
/* Host version: 1 for a console device (ioctl 4400h: bit 7, a device, and
   bit 0 or 1, console input or output). */
#include "dosfile.h"

aztec_isatty(fd)
{
	long info = dos_ioctl_info(fd);

	return info >= 0 && (info & 0x80) && (info & 3);
}
