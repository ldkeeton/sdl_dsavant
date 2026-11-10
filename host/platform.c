/* Host entry point: the SDL window and presenter on the main thread, the
   game on a thread of its own.

   The main thread plays the PC's hardware: it feeds key and mouse events to
   bios.c and redraws the window from the VGA framebuffer and DAC at the
   display's rate, with the mouse pointer on top.  The game thread runs
   ds_main and the overlays, unchanged.

   Environment:
     DS_GAME_DIR   where the data files are (default: the executable's dir)
     DS_ASPECT=0   square pixels, integer scaling (default: 4:3, as on a CRT)
     DS_SHOT=f.bmp[,secs...]  save the screen after secs (default 3), then
                   quit; with several times, f-SECS.bmp at each
     DS_SELFTEST=1 exercise the platform (selftest.c) instead of the game
     DS_KEYS=secs:keys[;secs:keys...]  type keys at those times; keys are
                   characters, SDL key names in braces ({Escape}
                   {Return} {Up} {F1} ...), or {click X Y}: a left click at
                   screen pixel (X, Y)
   Keys: F12 saves the screen (shotNNN.bmp), Alt+Enter toggles fullscreen.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "bios.h"
#include "dosfile.h"
#include "dosmem.h"
#include "overlay.h"
#include "platform.h"
#include "sound.h"

extern short ds_main(void);
void host_selftest(void);       /* selftest.c */

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int aspect = 1;          /* draw 320x200 as 4:3 */
static Uint32 wake_event;
static Uint32 exit_event;
static SDL_ThreadID main_thread;

void platform_wake(void)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = wake_event;
    SDL_PushEvent(&e);
}

static void platform_shutdown(int code)
{
    sound_shutdown();
    SDL_Quit();
    exit(code);
}

void host_exit(int code)
{
    fflush(stdout);
    fflush(stderr);
    if (SDL_GetCurrentThreadID() == main_thread)
        platform_shutdown(code);
    SDL_Event e;
    SDL_zero(e);
    e.type = exit_event;
    e.user.code = code;
    SDL_PushEvent(&e);
    for (;;)
        SDL_Delay(1000);
}

static int SDLCALL game_thread(void *data)
{
    (void)data;
    const char *t = SDL_getenv("DS_SELFTEST");
    if (t && t[0] == '1')
        host_selftest();
    overlay_init();
    ds_main();
    host_exit(0);
    return 0;
}

/* Logical (presentation) coordinates to screen pixels and back. */
static float logical_h(void) { return aspect ? 240.0f : (float)SCREEN_H; }

static void to_screen(float lx, float ly, float *x, float *y)
{
    *x = lx;
    *y = ly * SCREEN_H / logical_h();
}

static uint32_t frame_rgb[SCREEN_W * SCREEN_H];   /* the last frame drawn */

/* Save the last frame as a BMP (F12, or DS_SHOT). */
static void save_shot(const char *path)
{
    static int n;
    char name[64];
    if (!path) {
        snprintf(name, sizeof name, "shot%03d.bmp", n++);
        path = name;
    }
    SDL_Surface *s = SDL_CreateSurfaceFrom(SCREEN_W, SCREEN_H, SDL_PIXELFORMAT_XRGB8888,
                                           frame_rgb, SCREEN_W * 4);
    if (s && SDL_SaveBMP(s, path))
        fprintf(stderr, "saved %s\n", path);
    else
        fprintf(stderr, "screenshot %s: %s\n", path, SDL_GetError());
    SDL_DestroySurface(s);
}

static void draw_frame(void)
{
    uint32_t pal[256];

    for (int i = 0; i < 256; i++) {
        uint32_t r = vga_dac[i][0] & 63, g = vga_dac[i][1] & 63, b = vga_dac[i][2] & 63;
        r = r << 2 | r >> 4;
        g = g << 2 | g >> 4;
        b = b << 2 | b >> 4;
        pal[i] = 0xff000000u | r << 16 | g << 8 | b;
    }
    const uint8_t *fb = DOS_PTR(VGA_SEGMENT, 0);
    int graphics = vga_get_mode() == 0x13;
    if (graphics)
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            frame_rgb[i] = pal[fb[i]];
    else
        memset(frame_rgb, 0, sizeof frame_rgb);
    struct pointer_state p;
    if (graphics && bios_pointer(&p)) {
        for (int y = 0; y < p.h && p.y + y < SCREEN_H; y++) {
            uint32_t *row = frame_rgb + (p.y + y) * SCREEN_W;
            const uint8_t *src = p.image + (size_t)y * p.stride;
            for (int x = 0; x < p.w && p.x + x < SCREEN_W; x++)
                if (src[x] != 0xff)
                    row[p.x + x] = pal[src[x]];
        }
        SDL_HideCursor();
    } else {
        SDL_ShowCursor();
    }
    SDL_UpdateTexture(texture, 0, frame_rgb, SCREEN_W * 4);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_FRect dst = {0, 0, SCREEN_W, logical_h()};
    SDL_RenderTexture(renderer, texture, 0, &dst);
    SDL_RenderPresent(renderer);
}

static void handle(SDL_Event *e)
{
    if (e->type == SDL_EVENT_QUIT)
        platform_shutdown(0);
    if (e->type == exit_event)
        platform_shutdown(e->user.code);
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
        if (e->key.key == SDLK_RETURN && (e->key.mod & SDL_KMOD_ALT)) {
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            break;
        }
        if (e->key.key == SDLK_F12) {
            save_shot(0);
            break;
        }
        bios_key_event(&e->key);
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        float x, y;
        SDL_ConvertEventToRenderCoordinates(renderer, e);
        to_screen(e->motion.x, e->motion.y, &x, &y);
        bios_mouse_motion(x, y);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        float x, y;
        SDL_ConvertEventToRenderCoordinates(renderer, e);
        to_screen(e->button.x, e->button.y, &x, &y);
        bios_mouse_motion(x, y);
        bios_mouse_button(e->button.button, e->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        break;
    }
    default:
        break;
    }
}

/* DS_KEYS: key presses typed at set times (a debugging aid, like DS_SHOT). */
#define MAX_SCRIPTED 256
static struct {
    uint64_t at;
    SDL_Scancode sc;        /* SDL_SCANCODE_UNKNOWN: a click at x, y */
    SDL_Keymod mod;
    int x, y;
} scripted[MAX_SCRIPTED];
static int scripted_count, scripted_next;

static void script_keys(const char *spec, uint64_t start)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *part = strtok(buf, ";"); part; part = strtok(0, ";")) {
        char *colon = strchr(part, ':');
        if (!colon)
            continue;
        uint64_t at = start + (uint64_t)(atof(part) * 1e9);
        for (char *k = colon + 1; *k && scripted_count < MAX_SCRIPTED; k++, at += 100000000) {
            SDL_Keymod mod = 0;
            SDL_Scancode sc;
            int x, y;
            if (sscanf(k, "{click %d %d}", &x, &y) == 2) {
                scripted[scripted_count].at = at;
                scripted[scripted_count].sc = SDL_SCANCODE_UNKNOWN;
                scripted[scripted_count].x = x;
                scripted[scripted_count].y = y;
                scripted_count++;
                k = strchr(k, '}');
                continue;
            }
            if (*k == '{' && strchr(k, '}')) {
                char *end = strchr(k, '}');
                *end = 0;
                sc = SDL_GetScancodeFromName(k + 1);
                k = end;
            } else {
                sc = SDL_GetScancodeFromKey((SDL_Keycode)(unsigned char)*k, &mod);
            }
            if (sc == SDL_SCANCODE_UNKNOWN) {
                fprintf(stderr, "DS_KEYS: unknown key at \"%s\"\n", k);
                continue;
            }
            scripted[scripted_count].at = at;
            scripted[scripted_count].sc = sc;
            scripted[scripted_count].mod = mod;
            scripted_count++;
        }
    }
}

static void type_scripted(void)
{
    uint64_t now = SDL_GetTicksNS();
    while (scripted_next < scripted_count && scripted[scripted_next].at <= now) {
        if (scripted[scripted_next].sc == SDL_SCANCODE_UNKNOWN) {
            bios_mouse_motion((float)scripted[scripted_next].x, (float)scripted[scripted_next].y);
            bios_mouse_button(SDL_BUTTON_LEFT, 1);
            bios_mouse_button(SDL_BUTTON_LEFT, 0);
            scripted_next++;
            continue;
        }
        SDL_KeyboardEvent e;
        SDL_zero(e);
        e.type = SDL_EVENT_KEY_DOWN;
        e.down = true;
        e.scancode = scripted[scripted_next].sc;
        e.mod = scripted[scripted_next].mod;
        bios_key_event(&e);
        scripted_next++;
    }
}

static void take_warp(void)
{
    float x, y, wx, wy;
    if (!bios_take_warp(&x, &y))
        return;
    if (SDL_RenderCoordinatesToWindow(renderer, x, y * logical_h() / SCREEN_H, &wx, &wy))
        SDL_WarpMouseInWindow(window, wx, wy);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *a = SDL_getenv("DS_ASPECT");
    if (a && a[0] == '0')
        aspect = 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    main_thread = SDL_GetCurrentThreadID();
    wake_event = SDL_RegisterEvents(2);
    exit_event = wake_event + 1;

    if (!SDL_CreateWindowAndRenderer("Crusaders of the Dark Savant", SCREEN_W * 3,
                                     (int)logical_h() * 3, SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, (int)logical_h(),
                                     aspect ? SDL_LOGICAL_PRESENTATION_LETTERBOX
                                            : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(texture, aspect ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST);

    dosmem_init();
    const char *dir = SDL_getenv("DS_GAME_DIR");
    dosfile_init(dir ? dir : SDL_GetBasePath());
    bios_init();
    sound_init();

    if (!SDL_CreateThread(game_thread, "game", 0)) {
        fprintf(stderr, "SDL_CreateThread: %s\n", SDL_GetError());
        return 1;
    }

    /* DS_SHOT=file.bmp[,seconds...]: save a frame that long after startup
       (default 3 s), then quit.  With several times, file-SECS.bmp at
       each. */
    char shot_path[512] = "";
    double shot_secs[32];
    int shots = 0, shot_next = 0;
    uint64_t start = SDL_GetTicksNS();
    const char *shot = SDL_getenv("DS_SHOT");
    if (shot && *shot) {
        snprintf(shot_path, sizeof shot_path, "%s", shot);
        char *p = strchr(shot_path, ',');
        if (p)
            *p++ = 0;
        while (p && *p && shots < 32) {
            shot_secs[shots++] = atof(p);
            p = strchr(p, ',');
            if (p)
                p++;
        }
        if (!shots)
            shot_secs[shots++] = 3;
    }

    const char *keys = SDL_getenv("DS_KEYS");
    if (keys && *keys)
        script_keys(keys, SDL_GetTicksNS());

    const uint64_t frame = 1000000000ull / 60;
    uint64_t next = SDL_GetTicksNS();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handle(&e);
        take_warp();
        type_scripted();
        draw_frame();
        if (shot_next < shots && SDL_GetTicksNS() - start >= (uint64_t)(shot_secs[shot_next] * 1e9)) {
            char name[600];
            const char *dot = strrchr(shot_path, '.');
            if (shots == 1 || !dot)
                snprintf(name, sizeof name, "%s", shot_path);
            else
                snprintf(name, sizeof name, "%.*s-%g%s", (int)(dot - shot_path), shot_path,
                         shot_secs[shot_next], dot);
            save_shot(name);
            if (++shot_next == shots)
                platform_shutdown(0);
        }
        /* With vsync, present waited for the display; without it, keep to
           60 Hz, still taking events as they come. */
        next += frame;
        uint64_t now = SDL_GetTicksNS();
        if (next < now)
            next = now;
        while (now < next) {
            if (SDL_WaitEventTimeout(&e, (Sint32)((next - now) / 1000000)))
                handle(&e);
            now = SDL_GetTicksNS();
            if (next - now < 1000000)
                break;
        }
    }
}
