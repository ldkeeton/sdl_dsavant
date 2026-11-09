/* The drivers are linked in (shared/drv, vga/, play/), and the build
   leaves VGA.DRV and PLAY.DRV out, so there is nothing to load.  The
   driver blocks they were read into are still allocated
   (allocate_driver_blocks), so DOS memory looks as it did.  On DOS:
   load_video_driver.s, load_play_driver.s, driver_names.s and
   play_drv_name.s read the files, stopping the game if one was missing,
   and filled in the call slots and the video driver's copy of DS's block
   table and EMS state (which the driver now reads from dshw.h). */

load_video_driver(name)
char *name;
{
}

load_play_driver()
{
}
