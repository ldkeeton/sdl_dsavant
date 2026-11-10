/* SDL audio is also PLAY's PIT clock. One OPL sample is 24 PIT clocks;
   the sample interrupt (72 clocks) consequently runs every three samples.
   If opening audio fails, a thread advances exactly the same clock. */
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "sound.h"

#define SAMPLE_RATE 49716
static SDL_Mutex *lock;
static SDL_AudioStream *audio;
static SDL_Thread *fallback;
static SDL_AtomicInt stopping;
static void (*timer_handler)(void);
static uint32_t timer_period = 65536;
static int32_t timer_left = 65536;
static float pcm_level, dc_in, dc_out;

void sound_lock(void) { SDL_LockMutex(lock); }
void sound_unlock(void) { SDL_UnlockMutex(lock); }

void sound_timer(uint16_t divisor, void (*handler)(void))
{
    timer_period = divisor ? divisor : 65536;
    timer_left = (int32_t)timer_period;
    timer_handler = handler;
}

void sound_pcm(float level) { pcm_level = level; }

static void render(float *out, int count)
{
    sound_lock();
    for (int i = 0; i < count; i++) {
        timer_left -= 24;
        while (timer_left <= 0) {
            /* Reload before calling: the handler may reprogram the PIT. */
            timer_left += (int32_t)timer_period;
            if (timer_handler)
                timer_handler();
        }
        float in = host_opl_sample() / 32768.0f + pcm_level;
        /* Samples through the OPL are unipolar. Remove DC on every path. */
        float value = in - dc_in + 0.995f * dc_out;
        dc_in = in;
        dc_out = value;
        out[i] = SDL_clamp(value, -1.0f, 1.0f);
    }
    sound_unlock();
}

static void SDLCALL feed(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
    (void)userdata;
    (void)total;
    float buf[512];
    int count = (additional + (int)sizeof(float) - 1) / (int)sizeof(float);
    while (count > 0) {
        int n = SDL_min(count, (int)SDL_arraysize(buf));
        render(buf, n);
        if (!SDL_PutAudioStreamData(stream, buf, n * (int)sizeof(float)))
            break;
        count -= n;
    }
}

static int SDLCALL silent_clock(void *unused)
{
    (void)unused;
    uint64_t start = SDL_GetTicksNS(), rendered = 0;
    float buf[512];
    while (!SDL_GetAtomicInt(&stopping)) {
        uint64_t due = (SDL_GetTicksNS() - start) * SAMPLE_RATE / 1000000000ull;
        while (rendered < due) {
            int n = (int)SDL_min(due - rendered, SDL_arraysize(buf));
            render(buf, n);
            rendered += n;
        }
        SDL_Delay(1);
    }
    return 0;
}

void sound_init(void)
{
    lock = SDL_CreateMutex();
    if (!lock) {
        fprintf(stderr, "sound mutex: %s\n", SDL_GetError());
        exit(1);
    }
    host_opl_reset();
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, SAMPLE_RATE};
    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, NULL);
    if (audio && !SDL_ResumeAudioStreamDevice(audio)) {
        SDL_DestroyAudioStream(audio);
        audio = NULL;
    }
    if (!audio) {
        fprintf(stderr, "audio unavailable: %s; using silent sound clock\n", SDL_GetError());
        fallback = SDL_CreateThread(silent_clock, "sound clock", NULL);
        if (!fallback) {
            fprintf(stderr, "sound clock: %s\n", SDL_GetError());
            exit(1);
        }
    }
}

void sound_shutdown(void)
{
    SDL_SetAtomicInt(&stopping, 1);
    if (fallback)
        SDL_WaitThread(fallback, NULL);
    SDL_DestroyAudioStream(audio);
    /* The game thread can still be in a PLAY entry until process exit. */
}
