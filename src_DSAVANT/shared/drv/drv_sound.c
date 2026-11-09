/* Sounds and scores in the maze area, played by the PLAY driver.  On DOS
   VGA.DRV far-called PLAY.DRV's entries through pointers DS copied in
   (music_start_far ...); linked in, these call them by name.  On DOS:
   play_sound.s, wait_sound.s, stop_sound.s, play_score.s,
   is_music_playing.s, stop_music.s, set_music_tempo_volume.s. */
#include "drv.h"

extern unsigned *maze_block();

/* Sound n (a load_sound slot), with PLAY's three play arguments. */
play_sound(n, a, b, c)
unsigned n;
{
	unsigned *slot;

	if (n >= 20)
		return;
	slot = &sound_slots[n * 2];
	if (!slot[0])
		return;
	play_select_device(0, maze_mem_seg + maze_block(slot[0])[0], 0, slot[1], a, b, c);
}

stop_sound()
{
	play_select_device(0, 0, 0, 0, 0, 0, 0);
}

wait_sound(wait)
{
	wait_for_music(wait);
}

/* Score n (a load_score slot): its patches, then the tempo and volume
   (a, b), then the score from where its patches end. */
play_score(n, a, b, how)
unsigned n;
{
	unsigned *slot, seg;

	slot = &score_slots[n * 2];
	if (!slot[0])
		return;
	seg = maze_mem_seg + maze_block(slot[0])[0];
	music_set_data(0, slot[1] ? seg : 0, score_info[n]);
	music_set_tempo_volume(a, b);
	music_start(slot[1], seg, how);
}

is_music_playing()
{
	return music_is_playing();
}

stop_music()
{
	stop_music_playback();
}

set_music_tempo_volume(tempo, volume)
{
	music_set_tempo_volume(tempo, volume);
}
