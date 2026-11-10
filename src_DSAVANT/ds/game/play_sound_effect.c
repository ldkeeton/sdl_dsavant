/* Play a sound using its slot data and the configured device volume. */
#include "structs/sndslot.h"
#include "structs/loadrec.h"
extern struct load_header scenario_header;

extern struct sound_slot_info_rec sound_slot_info[];
extern int loaded_sound_id;
extern int game_state;
extern int call_play_sound();

play_sound_effect(sound_id)
int sound_id;
{
	int sound_number;
	int volume;
	int volume_index;
	int flags;

	volume = sound_slot_info[sound_id].volume;
	sound_number = sound_id;
	/* A zero size redirects to the record's alternate sound number. */
	if (sound_slot_info[sound_id].size.full_size == 0)
		sound_number = sound_slot_info[sound_id].alias_num;
	/* Double volume for a configured device, capped at 255. */
	if (scenario_header.sound_device > 0) {
		volume *= 2;
		if (volume > 255)
			volume = 255;
	}
	/* Monster and ambient sound IDs use their own volume settings. */
	volume_index = 0;
	if (sound_id == 16 && loaded_sound_id >= 30)
		volume_index = 1;
	else if (sound_id == 14 || sound_id == 15)
		volume_index = 2;
	volume = scenario_header.volumes[volume_index] * volume / 10;
	/* Combat advances while samples play; cleanup paths still wait before
	   replacing or freeing the memory used by the audio callback. */
	flags = sound_slot_info[sound_id].play_flags;
	if (scenario_header.battle_auto_enabled == 1 &&
	    (game_state == 10 || game_state == 11 ||
	     game_state == 13 || game_state == 14))
		flags &= ~1;
	if (volume > 0)
		call_play_sound(sound_number,
			sound_slot_info[sound_id].rate, volume,
			flags);
}
