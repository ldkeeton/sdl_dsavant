/* PLAY.DRV state, shared by the score player, OPL and sample handlers.
   Only the DOS OPL path is retained; MPU events are consumed silently.
   Every public entry and timer callback holds sound_lock(). */
#ifndef PLAY_H
#define PLAY_H
#include "dosmem.h"
#include "sound.h"
#include "bios.h"

struct play_voice {
    uint8_t velocity, semitone, index;
    uint16_t age;
};
extern struct play_voice opl_voices[11];
extern uint8_t music_device, play_device, music_enabled, sample_idle;
extern uint8_t playback_volume, opl_channel_index;
extern uint8_t opl_note_index, opl_note_velocity, opl_note_semitone;
extern uint16_t music_tempo, tick_divider, restart_delay;
extern uint16_t stream_segment, stream_offset, stream_start_offset;
extern uint16_t music_patch_segment, music_patch_offset, opl_pitch_value;
extern uint8_t loop_at_end, channel_patch_index[16];
extern uint8_t opl_register_shadow[256];
extern const uint8_t opl_velocity_scale_table[256];
extern const uint16_t opl_fnum_table[192];

void play_opl_write(uint8_t reg, uint8_t value);
void play_opl_init(void);
void play_opl_keys_off(void);
void play_opl_load_patch(void);
void play_opl_note_on(void);
void play_opl_note_off(void);
void play_opl_pulse(void);
void play_music_tick(void);
void play_music_interrupt(void);
void play_sample_interrupt(void);
void play_sample_finish(void);
#endif
