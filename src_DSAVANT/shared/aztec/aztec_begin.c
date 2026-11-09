/* Copyright (C) 1983 by Manx Software Systems */
/* Host version of the startup module: the host's main sets up the program
   and calls the game's main (host/platform.c), so what is left is errno and
   the exit call (int 21h/4Ch). */
#include "bios.h"

int errno;

aztec_impl_exit(code)
{
	host_exit(code & 0xff);
}
