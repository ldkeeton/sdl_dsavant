/* delay_ms(ms): waits ms milliseconds.  The original spun a loop calibrated
   against the BIOS tick (calibrate_loops_per_ms); the host sleeps.  As
   there, 0 counts down from 65536. */
#include "bios.h"

delay_ms(ms)
unsigned ms;
{
	host_delay_ms(ms ? ms : 65536L);
}

/* The far entry the video driver calls through delay_hook. */
far_delay_ms(ms)
unsigned ms;
{
	delay_ms(ms);
}
