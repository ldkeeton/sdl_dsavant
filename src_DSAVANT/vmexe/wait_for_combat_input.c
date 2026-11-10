/* Wait for combat input or the requested tick deadline. */
#include "structs/loadrec.h"
extern struct load_header scenario_header;
extern long far *tick_counter_ptr;
extern int flush_keyboard();
extern int poll_fullscreen_hotspot();
extern int poll_filtered_key();
extern unsigned char get_filtered_key();
extern int step_combat_screen_animation();
extern int combat_wait_occurred;
extern int combat_auto_advance;

wait_for_combat_input(timeout_ticks)
int timeout_ticks;
{
	long deadline;

	combat_wait_occurred = 1;
	/* A click or Enter fast forwards subsequent messages in this round.
	   Keep one tick per message so the text reaches the screen. */
	if (scenario_header.battle_auto_enabled == 1 &&
	    combat_auto_advance && timeout_ticks > 1)
		timeout_ticks = 1;
	flush_keyboard();
	deadline = *tick_counter_ptr + timeout_ticks;
	while (*tick_counter_ptr < deadline) {
		if (poll_fullscreen_hotspot()) {
			if (scenario_header.battle_auto_enabled == 1)
				combat_auto_advance = 1;
			return;
		}
		if (poll_filtered_key(1) && get_filtered_key(1) == 13) {
			if (scenario_header.battle_auto_enabled == 1)
				combat_auto_advance = 1;
			return;
		}
		step_combat_screen_animation();
	}
}
