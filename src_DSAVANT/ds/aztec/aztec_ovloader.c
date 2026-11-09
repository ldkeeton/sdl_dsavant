/* Copyright (C) 1984 by Manx Software Systems */
/* Host version: the overlays are linked into DS (tools/ovlink.py), so
   "loading" one resets its data and runs its ovmain.  Returns what ovmain
   returns, as the original jumped to the overlay's entry. */
#include "overlay.h"

aztec_ovloader(name)
char *name;
{
	return overlay_run(name);
}
