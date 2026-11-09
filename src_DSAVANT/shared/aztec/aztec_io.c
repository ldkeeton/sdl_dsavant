/* Copyright (C) 1983 1984 by Manx Software Systems */
/* Host version: the int 21h file calls are dosfile.c's.  A call that fails
   sets errno to the DOS error code and returns -1, as the original did when
   DOS set carry.  Handles flagged as ttys in _ioflg go through the tty
   routines (aztec_ttyio.c), as before. */
#include "dosfile.h"

extern int errno;
char _ioflg[20];

static int
dos_result(r)
long r;
{
	if (r < 0) {
		errno = -r;
		return -1;
	}
	return r;
}

aztec_impl_read(fd, buff, len)
char *buff; unsigned len;
{
	return dos_result(dos_read(fd, buff, len));
}

aztec_impl_write(fd, buff, len)
char *buff; unsigned len;
{
	return dos_result(dos_write(fd, buff, len));
}

aztec_read(fd, buffer, len)
char *buffer; unsigned len;
{
	if ((unsigned)fd < 20 && _ioflg[fd])
		return aztec_impl_tty_rd(fd, buffer, len);
	return aztec_impl_read(fd, buffer, len);
}

aztec_write(fd, buffer, len)
char *buffer; unsigned len;
{
	if ((unsigned)fd < 20 && _ioflg[fd])
		return aztec_impl_tty_wr(fd, buffer, len);
	return aztec_impl_write(fd, buffer, len);
}

aztec_close(fd)
{
	return dos_result(dos_close(fd)) < 0 ? -1 : 0;
}

long
aztec_lseek(fd, pos, how)
long pos;
{
	long r = dos_lseek(fd, pos, how);

	if (r < 0) {
		errno = -r;
		return -1;
	}
	return r;
}

aztec_unlink(name)
char *name;
{
	return dos_result(dos_unlink(name)) < 0 ? -1 : 0;
}
