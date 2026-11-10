/* Far pointer to the BIOS tick counter, 0000:046c (the host's dos_mem). */
#include "bios.h"

volatile long far *tick_counter_ptr = (volatile long far *)(dos_mem + BIOS_TICK_ADDR);
