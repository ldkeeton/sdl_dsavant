/* PLAY's music stream and voice allocator. This is the reachable melodic
   path of music_player_tick.s; its unconditional jumps bypass rhythm and
   special-note handling. MPU-401 output is dropped, its events still step. */
#include "play.h"

struct play_voice opl_voices[11];
uint8_t music_device = 1, play_device, music_enabled, sample_idle = 0xff;
uint8_t playback_volume = 0x80, loop_at_end;
uint16_t music_tempo = 100, tick_divider = 100, restart_delay = 1;
uint16_t stream_segment, stream_offset, stream_start_offset;
uint16_t music_patch_segment, music_patch_offset;
uint8_t channel_patch_index[16] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};
static uint8_t channel_last_byte[16] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static uint8_t stream_byte(uint16_t *pos)
{
    uint8_t value = *DOS_PTR(stream_segment, *pos);
    ++*pos;
    return value;
}

static uint16_t stream_word(uint16_t *pos)
{
    uint16_t lo = stream_byte(pos);
    return lo | ((uint16_t)stream_byte(pos) << 8);
}

static uint8_t is_opl(void) { return music_device == 1 || music_device == 4; }

static void note_event(uint8_t event, uint16_t *pos)
{
    uint8_t patch = channel_patch_index[event & 15];
    uint8_t semitone = stream_byte(pos), velocity = stream_byte(pos);
    uint16_t oldest;
    int i, voice;
    uint8_t index = patch * 4;
    if (play_device != 5) {
        semitone -= 12;
        velocity = (velocity >> 2) + 0x60;
    }
    opl_note_semitone = semitone;
    opl_note_velocity = velocity;
    opl_note_index = index;
    if (patch == 255) return;
    if (event >= 0x10 && velocity) {
        for (i = 0; i < 11; ++i) ++opl_voices[i].age;
        voice = -1;
        oldest = 0;
        for (i = 0; i < 8; ++i)
            if (!opl_voices[i].velocity && opl_voices[i].age >= oldest) {
                oldest = opl_voices[i].age;
                voice = i;
            }
        if (voice < 0) {
            oldest = 0;
            for (i = 0; i < 8; ++i)
                if (opl_voices[i].index == index && opl_voices[i].age >= oldest) {
                    oldest = opl_voices[i].age;
                    voice = i;
                }
            /* DOS jumps straight to note_write_voice here: it keeps the
               previous OPL channel and does not reload the patch. */
            if (voice >= 0) goto write_voice;
            oldest = 0;
            voice = 0;
            for (i = 0; i < 8; ++i)
                if (opl_voices[i].age >= oldest) {
                    oldest = opl_voices[i].age;
                    voice = i;
                }
        }
        opl_channel_index = voice;
        if (opl_voices[voice].index != index && is_opl())
            play_opl_load_patch();
write_voice:
        opl_voices[voice].velocity = velocity;
        opl_voices[voice].semitone = semitone;
        opl_voices[voice].index = index;
        opl_voices[voice].age = 0;
        if (is_opl()) play_opl_note_on();
    } else {
        for (i = 0; i < 8; ++i)
            if (opl_voices[i].velocity && opl_voices[i].semitone == semitone &&
                opl_voices[i].index == index) {
                opl_voices[i].velocity = 0;
                opl_voices[i].age = 0;
                opl_channel_index = i;
                if (is_opl()) play_opl_note_off();
            }
    }
}

void play_music_tick(void)
{
    uint16_t pos, delay;
    uint8_t event, cmd, channel;
    uint32_t budget = 65536;
    static const uint8_t event_counts[7] = {3,3,3,3,2,2,3};
    tick_divider = music_tempo;
    if (!music_enabled || --restart_delay) goto done;
    pos = stream_offset;
    /* A corrupt or empty looping score must not trap the audio thread. */
    while (budget--) {
        event = stream_byte(&pos);
        if (event == 0xfe) {
            delay = stream_word(&pos);
            if (!delay) continue;
            restart_delay = delay;
            stream_offset = pos;
            goto done;
        }
        if (event == 0xff || event >= 0x70) {
            if (loop_at_end) { pos = stream_start_offset; continue; }
            music_enabled = 0;
            if (sample_idle) sound_timer(0, 0);
            goto done;
        }
        if (music_device == 5) {
            cmd = event >> 4;
            channel = event & 15;
            if (cmd == 4)
                channel_last_byte[channel] = stream_byte(&pos);
            else if (channel_last_byte[channel] == 0x3e)
                pos += 2; /* empty remap table: skip the unmapped pair */
            else
                pos += event_counts[cmd] - 1;
        } else if (event < 0x20) {
            note_event(event, &pos);
        } else if (event < 0x30) {
            stream_word(&pos);
        } else if (event < 0x50) {
            channel_patch_index[event & 15] = stream_byte(&pos);
        } else if (event < 0x60) {
            stream_byte(&pos);
        } else {
            opl_pitch_value = stream_word(&pos);
        }
    }
    music_enabled = 0;
    if (is_opl()) play_opl_keys_off();
    if (sample_idle) sound_timer(0, 0);
done:
    if (play_device == 1) host_opl_address(0x40);
}

void play_music_interrupt(void) { play_music_tick(); }

music_set_data(offset, segment, device)
unsigned offset, segment, device;
{
    sound_lock();
    music_patch_offset = offset;
    music_patch_segment = segment;
    music_device = device < 6 ? device : 0;
    sound_unlock();
}

music_set_tempo_volume(tempo, volume)
unsigned tempo, volume;
{
    sound_lock();
    music_tempo = tempo;
    playback_volume = volume;
    /* On 286+ PLAY never reprograms the running timer here. */
    sound_unlock();
}

music_start(offset, segment, loop)
unsigned offset, segment, loop;
{
    int i;
    sound_lock();
    music_enabled = 0;
    stream_offset = stream_start_offset = offset;
    stream_segment = segment;
    loop_at_end = loop;
    tick_divider = restart_delay = 1;
    for (i = 0; i < 11; ++i) {
        opl_voices[i].velocity = opl_voices[i].semitone = 0;
        opl_voices[i].index = 255;
        opl_voices[i].age = 0;
    }
    if (is_opl()) play_opl_init();
    /* An installed sample interrupt already supplies the music clock. */
    if (sample_idle)
        sound_timer((uint8_t)music_tempo * 72, play_music_interrupt);
    music_enabled = 1;
    sound_unlock();
    return 0;
}

stop_music_playback()
{
    sound_lock();
    if (music_enabled) {
        music_enabled = 0;
        if (sample_idle) sound_timer(0, 0);
        if (is_opl()) play_opl_keys_off();
    }
    sound_unlock();
}

music_is_playing()
{
    int result;
    sound_lock();
    result = music_enabled ? ((uint8_t)stream_offset ? (uint8_t)stream_offset : 1) : 0;
    sound_unlock();
    return result;
}

wait_for_music(wait)
{
    int pending;
    do {
        sound_lock();
        pending = wait ? (!sample_idle || music_enabled) : (play_device == 0 && !sample_idle);
        sound_unlock();
        if (pending) host_delay_ms(1);
    } while (pending);
}
