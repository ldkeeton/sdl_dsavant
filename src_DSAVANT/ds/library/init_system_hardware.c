/* Brings up the machine: seeds the random generator from the BIOS tick,
   picks the video adapter, allocates the driver blocks, finds the mouse
   and EMS, loads the drivers and clears the screen.  Returns the status
   word: 1 VGA, 2 EGA (0 neither), plus 1000h if there is a mouse.
   mode is the scenario's graphics setting: bit 0 allows VGA; 4 means no
   mouse. */

init_system_hardware(mode)
{
	int result;
	char *driver_name;

	seed_random_from_timer();
	calibrate_loops_per_ms();
	detect_video_adapter(mode, &result, &driver_name);
	allocate_driver_blocks(result);
	detect_mouse(mode, &result);
	detect_expanded_memory();
	load_play_driver();
	load_video_driver(driver_name);
	call_fill_screen_buffer(0);
	return result;
}
