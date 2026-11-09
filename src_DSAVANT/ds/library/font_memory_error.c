/* The fatal errors of the hardware layer: DOS 09h prints a $-terminated
   message, then the program ends with status 1 (DOS 4Ch). */
#include "dosfile.h"
#include "bios.h"

dos_fatal(msg)
char *msg;
{
	int n;

	for (n = 0; msg[n] != '$'; n++)
		;
	dos_write(1, msg, n);
	host_exit(1);
}

font_memory_error()
{
	dos_fatal("\007\r\nMemory error loading font\r\n$");
}
