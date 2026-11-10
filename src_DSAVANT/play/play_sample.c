/* PLAY's PIT sample handlers, output tables and device-selection entry.
   PC speaker/LPT output is mixed as PCM; AdLib samples modulate the OPL's
   held operator exactly as the original driver did. */
#include "play.h"

static uint16_t sample_segment, sample_offset;
static uint8_t sample_step, sample_fraction;
static uint8_t sample_table[256], sample_position, sample_limit;
static int8_t sample_ramp;

static void build_sample_table(uint8_t volume)
{
    uint16_t i, center, scaled;
    int16_t level;
    if (play_device == 1 || play_device == 4) {
        center = volume < 128 ? volume : 128;
        for (i = 0; i < 128; ++i) {
            scaled = (127 - i) * (uint16_t)(volume * 2) >> 8;
            if (scaled >= 128) scaled = 127;
            sample_table[i] = opl_velocity_scale_table[center - scaled - 1];
            scaled = i * (uint16_t)(volume * 2) >> 8;
            if (scaled >= 128) scaled = 127;
            sample_table[i + 128] = opl_velocity_scale_table[center + scaled];
        }
    } else {
        for (i = 0; i < 256; ++i) {
            level = (uint16_t)volume * i >> 8;
            if (play_device == 0) {
                /* DOS uses the sign bit after the byte subtraction. */
                level = (int8_t)(level - (volume < 73 ? 0 : (volume - 73) / 2));
                if (level < 0) level = 0;
                else if (level >= volume) level = volume;
                ++level;
            }
            sample_table[i] = level;
        }
    }
}

void play_sample_finish(void)
{
    sample_idle = 255;
    sound_pcm(0);
    if (music_enabled)
        sound_timer((uint8_t)music_tempo * 72, play_music_interrupt);
    else
        sound_timer(0, 0);
}

static void sample_output(uint8_t sample)
{
    uint8_t level = sample_table[sample];
    if (play_device == 1)
        host_opl_data(level); /* register latch is significant */
    else if (play_device == 0)
        sound_pcm(level / 72.0f);
    else
        sound_pcm(level / 255.0f);
}

void play_sample_interrupt(void)
{
    uint16_t sum;
    uint8_t value;
    if (play_device == 1 && sample_ramp < 0) {
        value = sample_position;
        if (value == sample_limit) { sample_ramp = 0; return; }
        ++sample_position;
        sample_output(value);
    } else if (play_device == 1 && sample_ramp > 0) {
        value = sample_position;
        if (!value) {
            host_opl_address(0x3f);
            play_sample_finish();
            return;
        }
        --sample_position;
        sample_output(value);
    } else {
        value = *DOS_PTR(sample_segment, sample_offset);
        sample_output(value);
        sum = (uint16_t)sample_fraction + sample_step;
        sample_fraction = sum;
        if (sum > 255 && ++sample_offset == 0) {
            if (play_device == 1) {
                sample_position = *DOS_PTR(sample_segment, 0xffff);
                sample_ramp = 1;
            } else {
                play_sample_finish();
            }
            return;
        }
    }
    if (--tick_divider == 0) play_music_tick();
}

static void wait_sample(void)
{
    int pending;
    do {
        sound_lock();
        pending = !sample_idle;
        sound_unlock();
        if (pending) host_delay_ms(1);
    } while (pending);
}

play_select_device(low, high, start, end, rate, volume, flags)
unsigned low, high, start, end, rate, volume, flags;
{
    uint8_t device;
    sound_lock();
    if ((uint8_t)flags == 255) {
        device = flags >> 8;
        if (device >= 6) device = 0;
        if (device == 4) device = 1;
        play_device = device;
        tick_divider = 100;
        if (device == 1) play_opl_pulse();
        sound_unlock();
        return 0;
    }
    if (!(uint8_t)volume) {
        stop_music_playback();
        sound_unlock();
        wait_sample();
        sound_lock();
        sound_timer(0, 0);
        sample_idle = 255;
        sound_pcm(0);
        sound_unlock();
        return 0;
    }
    if ((flags & 2) && !sample_idle) {
        sound_unlock();
        return 0;
    }
    if (play_device == 1) sample_ramp = sample_idle ? -1 : 0;
    /* The word addition wraps before the paragraph shift, as on 8086. */
    sample_segment = ((uint16_t)(low + end) >> 4) + high - 0x1000;
    sample_offset = ((uint16_t)(start - end) & 0xfffe) + 0x10;
    sample_step = 255 - (uint8_t)rate;
    sample_limit = *DOS_PTR(sample_segment, sample_offset);
    sample_position = 0;
    build_sample_table(volume);
    if (play_device == 1 || play_device == 4) host_opl_address(0x40);
    if (sample_idle) sound_timer(72, play_sample_interrupt);
    sample_idle = 0;
    sound_unlock();
    if (flags & 1) wait_sample();
    return 0;
}
