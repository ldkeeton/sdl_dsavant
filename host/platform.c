/* Host entry point.  (Phase 1: runs the game on the main thread; the SDL
   window, presenter and game thread come in phase 2.) */
#include <stdio.h>
#include "overlay.h"

extern short ds_main(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    overlay_init();
    ds_main();
    return 0;
}
