/* Deterministic PLAY checks against the built, rewritten game objects and
   the real ymfm core. The hardware doubles let waits advance virtual time. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma pack(push, 1)
#include "play.h"
#pragma pack(pop)

extern short music_set_data(int, int, int);
extern short music_set_tempo_volume(int, int);
extern short music_start(int, int, int);
extern short stop_music_playback(void);
extern short music_is_playing(void);
extern short wait_for_music(int);
extern short play_select_device(int, int, int, int, int, int, int);

uint8_t dos_mem[DOS_MEM_SIZE];
static void (*interrupt)(void);
static uint32_t period;
static int32_t remaining;
static float pcm;
static int nesting;

void sound_lock(void) { ++nesting; }
void sound_unlock(void) { assert(nesting > 0); --nesting; }
void sound_timer(uint16_t divisor, void (*handler)(void))
{
    period = divisor ? divisor : 65536;
    remaining = period;
    interrupt = handler;
}
void sound_pcm(float value) { pcm = value; }
static double advance(unsigned samples)
{
    double energy = 0;
    while (samples--) {
        remaining -= 24;
        while (remaining <= 0) {
            remaining += period;
            if (interrupt) interrupt();
        }
        double value = host_opl_sample() / 32768.0 + pcm;
        energy += value * value;
    }
    return energy;
}
void host_delay_ms(uint32_t ms) { advance(ms * 50); }

static void score_fixture(const uint8_t *score, size_t size)
{
    stop_music_playback();
    memset(DOS_PTR(0x4000, 0), 0, 1024);
    /* One simple, sustained sine patch. */
    uint8_t patch[] = {0x21,0,0xf0,0x0f,0,0x21,0,0xf0,0x0f,0,1};
    memcpy(DOS_PTR(0x4000, 0x40), patch, sizeof patch);
    memcpy(DOS_PTR(0x4000, 0x100), score, size);
    music_set_data(0, 0x4000, 1);
    music_set_tempo_volume(100, 255);
    music_start(0x100, 0x4000, 0);
}

static void music(void)
{
    const uint8_t score[] = {0x30,0, 0x10,12,255, 0xfe,2,0,
                             0x00,12,0, 0xfe,1,0, 0xff};
    play_select_device(0,0,0,0,0,0,0x1ff);
    score_fixture(score, sizeof score);
    assert(period == 7200 && music_is_playing());
    advance(299);
    assert(stream_offset == 0x100);
    advance(1);
    assert(stream_offset == 0x108 && restart_delay == 2);
    assert(opl_voices[7].velocity == 159 && opl_voices[7].index == 0);
    assert(opl_register_shadow[0xa8] == 0x59 && opl_register_shadow[0xb8] == 0x21);
    assert((opl_register_shadow[0x55] & 63) == 48);
    assert(advance(300) > 0.000001);
    assert(opl_voices[7].velocity);
    advance(300);
    assert(!opl_voices[7].velocity && !(opl_register_shadow[0xb8] & 0x20));
    advance(300);
    assert(!music_is_playing() && !interrupt);

    uint8_t crowded[64] = {0x30,0};
    unsigned n = 2;
    for (int i = 0; i < 9; ++i) {
        crowded[n++] = 0x10;
        crowded[n++] = 12+i;
        crowded[n++] = 255;
    }
    crowded[n++] = 0xfe; crowded[n++] = 1; crowded[n++] = 0;
    crowded[n++] = 0xff;
    score_fixture(crowded, n);
    advance(300);
    assert(opl_voices[7].semitone == 8 && opl_channel_index == 0);
    stop_music_playback();
    for (int i = 0xb1; i <= 0xb9; ++i) assert(!(opl_register_shadow[i] & 0x20));

    score_fixture(score, sizeof score);
    music_start(0x100, 0x4000, 1);
    advance(300*5);
    assert(music_is_playing());
    stop_music_playback();
    music_set_tempo_volume(256,255);
    music_start(0x100,0x4000,0);
    assert(period == 65536); /* 8-bit tempo multiplication */
    stop_music_playback();
    puts("music: events, PIT timing, levels, voice stealing, loop and stop passed");
}

static void samples(void)
{
    memset(DOS_PTR(0x3000, 16), 128, 16);
    play_select_device(0,0,0,0,0,0,0x2ff);
    play_select_device(0,0x3000,0,32,127,255,0);
    assert(!sample_idle && period == 72);
    advance(48);
    assert(!sample_idle && pcm > 0);
    play_select_device(0,0x3000,0,32,127,255,2); /* skip busy */
    advance(48);
    assert(sample_idle && !interrupt && pcm == 0);
    play_select_device(0,0x3000,0,32,127,255,1); /* blocking wait */
    assert(sample_idle && !interrupt);

    const uint8_t score[] = {0xfe,100,0,0xff};
    score_fixture(score, sizeof score);
    play_select_device(0,0x3000,0,32,127,255,0);
    advance(96);
    assert(sample_idle && music_enabled && period == 7200);
    play_select_device(0,0,0,0,0,0,0);
    assert(!music_enabled && !interrupt);

    play_select_device(0,0,0,0,0,0,0x1ff);
    play_select_device(0,0x3000,0,32,127,255,0);
    assert(advance(300) > 0.000001); /* ramp and OPL sample modulation */
    advance(1000);
    assert(sample_idle && !interrupt);
    play_select_device(0,0,0,0,0,0,0x0ff);
    play_select_device(0,0x3000,0,32,127,100,0);
    wait_for_music(0);
    assert(sample_idle && !interrupt);
    puts("samples: segment addressing, fractional rate, ramps, wait/skip and music resume passed");
}

static size_t load(const char *dir, int n, const char *ext, uint8_t *dst)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SCORE%02d.%s", dir, n, ext);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    size_t size = fread(dst, 1, 32768, f);
    assert(feof(f));
    fclose(f);
    return size;
}

static void real_scores(const char *dir)
{
    for (int n = 0; n < 6; ++n) {
        size_t patch = load(dir, n, "PBA", DOS_PTR(0x4000,0));
        load(dir, n, "CSF", DOS_PTR(0x4000,patch));
        music_set_data(0,0x4000,1);
        music_set_tempo_volume(100,255);
        music_start(patch,0x4000,0);
        double energy = advance(49716);
        assert(energy > 0.000001);
        unsigned ticks = 0;
        while (music_is_playing() && ticks++ < 100000) play_music_tick();
        assert(!music_is_playing());
        printf("SCORE%02d: audible PCM, stream ended after %u further ticks\n", n, ticks);
        /* PBL/MPU streams consume the same CSF without sending MIDI. */
        load(dir, n, "PBL", DOS_PTR(0x4000,0));
        load(dir, n, "CSF", DOS_PTR(0x5000,0));
        music_set_data(0,0x4000,5);
        music_start(0,0x5000,0);
        ticks = 0;
        while (music_is_playing() && ticks++ < 100000) play_music_tick();
        assert(!music_is_playing());
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    host_opl_reset();
    sound_timer(0,0);
    music();
    samples();
    real_scores(argv[1]);
    assert(!nesting);
    puts("PLAY checks passed");
    return 0;
}
