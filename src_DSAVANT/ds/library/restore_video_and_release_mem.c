/* On exit: the startup video mode back, the driver, font and EMS memory
   released. */

restore_video_and_release_mem()
{
	restore_saved_video_state();
	free_driver_and_font_memory();
	release_expanded_memory();
}
