/* DS's calls into the drivers.  On DOS each far-called its entry in the
   loaded VGA.DRV or PLAY.DRV through a slot table load_video_driver and
   load_play_driver filled in, the driver reading the caller's arguments
   off the stack; linked in, they are plain calls with those arguments.
   On DOS: call_thunks_area.s, call_thunks_media.s, call_thunks_screen.s,
   call_thunks_text_pos.s. */

/* Screen areas. */
call_show_area(x1, y1, x2, y2) { return show_area(x1, y1, x2, y2); }
call_fill_screen_buffer(c) { return fill_screen_buffer(c); }
call_put_pixel(x, y, c) { return put_pixel(x, y, c); }
call_fill_area(x1, y1, x2, y2, c) { return fill_area(x1, y1, x2, y2, c); }
call_invert_area(x1, y1, x2, y2) { return invert_area(x1, y1, x2, y2); }
long call_get_free_space(kind, drive) { return get_free_space(kind, drive); }
long call_save_area(x1, y1, x2, y2) { return save_area(x1, y1, x2, y2); }
long call_cache_area(x1, y1, x2, y2, mode, fd) { return cache_area(x1, y1, x2, y2, mode, fd); }
call_restore_area(area, release) long area; { return restore_area(area, release); }

/* handle is what cache_area returned: its mode, then its slot. */
call_restore_screen(handle, release, fd)
long handle;
{
	return restore_screen((unsigned)handle, (unsigned)(handle >> 16), release, fd);
}

/* Sound and music. */
call_load_sound(fd, n) { return load_sound(fd, n); }
call_free_sound(n) { return free_sound(n); }
call_play_sound(n, a, b, c) { return play_sound(n, a, b, c); }
call_wait_sound(wait) { return wait_sound(wait); }
call_stop_sound() { return stop_sound(); }
call_load_score(fd, patch_fd, info, n) { return load_score(fd, patch_fd, info, n); }
call_play_score(n, a, b, how) { return play_score(n, a, b, how); }
call_free_score(n) { return free_score(n); }
call_is_music_playing() { return is_music_playing(); }
call_stop_music() { return stop_music(); }
call_set_music_tempo_volume(tempo, volume) { return set_music_tempo_volume(tempo, volume); }

/* PLAY's device selection: the sound card's port and type. */
call_play_select_device(port, device)
{
	return play_select_device(0, 0, 0, 0, 0, port, (device << 8) | 0xff);
}

/* Pictures, screens and mazes. */
call_load_picture(fd, n) { return load_picture(fd, n); }
call_free_picture(n) { return free_picture(n); }
call_draw_text(pic, x, y, cx1, cx2, cy1, cy2, mode, frames, add, percent)
char *frames;
{
	return draw_text(pic, x, y, cx1, cx2, cy1, cy2, mode, frames, add, percent);
}
call_load_screen(n, mode) { return load_screen(n, mode); }
call_free_screen(n) { return free_screen(n); }
call_save_screen(n) { return save_screen(n); }
call_dissolve_area(x1, y1, x2, y2, delay) { return dissolve_area(x1, y1, x2, y2, delay); }
call_load_maze(fd, n, from_ems) { return load_maze(fd, n, from_ems); }
call_free_maze(n) { return free_maze(n); }
call_draw_maze_piece(n, row, mode, piece) { return draw_maze_piece(n, row, mode, piece); }
call_cache_maze(fd, n, copy) { return cache_maze(fd, n, copy); }
call_cache_screen_to_storage(fd, n, copy) { return cache_screen_to_storage(fd, n, copy); }

/* Text. */
call_set_text_pos(x, y) { return set_text_pos(x, y); }
call_get_text_pos(x, y) int *x, *y; { return get_text_pos(x, y); }
call_set_text_color(fg, bg) { return set_text_color(fg, bg); }
call_set_text_opaque(on) { return set_text_opaque(on); }

/* Palette. */
call_set_rgb(i, r, g, b) { return set_rgb(i, r, g, b); }
call_get_rgb(i, r, g, b) int *r, *g, *b; { return get_rgb(i, r, g, b); }
call_fade_in(delay) { return fade_in(delay); }
call_fade_out(r, g, b, delay) { return fade_out(r, g, b, delay); }
call_reset_rgb(r, g, b) { return reset_rgb(r, g, b); }
call_get_palette(rgb) char *rgb; { return get_palette(rgb); }
call_set_palette(rgb, map) char *rgb, *map; { return set_palette(rgb, map); }

/* Mouse. */
call_mouse_init() { return mouse_init(); }
call_mouse_off() { return mouse_off(); }
call_mouse_hit_test(rects) int *rects; { return mouse_hit_test(rects); }
call_get_mouse(x, y) int *x, *y; { return get_mouse(x, y); }
call_set_mouse_pointer(image, stride, height, x, y)
char *image;
{
	return set_mouse_pointer(image, stride, height, x, y);
}
call_mouse_glide_to(x, y, delay) { return mouse_glide_to(x, y, delay); }
call_mouse_glide_back(x, y, delay) { return mouse_glide_back(x, y, delay); }
