/* Emulated PC hardware and BIOS (bios.h): tick counter, keyboard buffer,
   int 33h mouse, VGA DAC and retrace.

   The game thread calls the bios.h functions; the SDL thread feeds them
   through platform.h.  The key buffer and the mouse state are shared under
   one lock. */
#include <string.h>
#include <SDL3/SDL.h>
#include "bios.h"
#include "dosmem.h"
#include "platform.h"

uint8_t vga_dac[256][3];

static SDL_Mutex *lock;
static SDL_Condition *key_arrived;

/* ---- timer ------------------------------------------------------------------ */

static uint64_t start_ns;
static int32_t start_ticks;

static void update_ticks(void)
{
    /* The PIT runs at 1193182 Hz and the BIOS counts every 65536th pulse. */
    uint64_t us = (SDL_GetTicksNS() - start_ns) / 1000;
    BIOS_TICKS = start_ticks + (int32_t)(us * 1193182 / 65536 / 1000000);
}

static Uint64 SDLCALL tick_timer(void *data, SDL_TimerID id, Uint64 interval)
{
    (void)data;
    (void)id;
    update_ticks();
    return interval;
}

void host_delay_ms(uint32_t ms)
{
    SDL_Delay(ms);
}

/* ---- keyboard --------------------------------------------------------------- */

#define KEYBUF 16
static uint16_t keybuf[KEYBUF];
static int key_head, key_count;
static int pending_scan = -1;   /* dos_getch: the scan code of an extended key */

static int key_take_locked(void)
{
    uint16_t k = keybuf[key_head];
    key_head = (key_head + 1) % KEYBUF;
    key_count--;
    return k;
}

int32_t bios_key_peek(void)
{
    int32_t k = -1;
    SDL_LockMutex(lock);
    if (key_count)
        k = keybuf[key_head];
    SDL_UnlockMutex(lock);
    if (k < 0)
        SDL_Delay(1);       /* the game polls in a loop: don't take a whole core */
    return k;
}

uint16_t bios_key_read(void)
{
    uint16_t k;
    SDL_LockMutex(lock);
    while (!key_count)
        SDL_WaitConditionTimeout(key_arrived, lock, 100);
    k = (uint16_t)key_take_locked();
    SDL_UnlockMutex(lock);
    return k;
}

uint8_t dos_getch(void)
{
    if (pending_scan >= 0) {
        uint8_t s = (uint8_t)pending_scan;
        pending_scan = -1;
        return s;
    }
    uint16_t k = bios_key_read();
    if ((k & 0xff) == 0)
        pending_scan = k >> 8;
    return (uint8_t)k;
}

void bios_key_flush(void)
{
    SDL_LockMutex(lock);
    key_count = 0;
    SDL_UnlockMutex(lock);
    pending_scan = -1;
}

/* PC scan code (set 1) and ASCII, normal / shifted / with Ctrl, of each key.
   ASCII 0 makes an extended key. */
struct keydef { uint8_t scan, lo, up, ctl; };

static const struct keydef keys[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_ESCAPE] = {0x01, 27, 27, 27},
    [SDL_SCANCODE_1] = {0x02, '1', '!', 0},
    [SDL_SCANCODE_2] = {0x03, '2', '@', 0},
    [SDL_SCANCODE_3] = {0x04, '3', '#', 0},
    [SDL_SCANCODE_4] = {0x05, '4', '$', 0},
    [SDL_SCANCODE_5] = {0x06, '5', '%', 0},
    [SDL_SCANCODE_6] = {0x07, '6', '^', 30},
    [SDL_SCANCODE_7] = {0x08, '7', '&', 0},
    [SDL_SCANCODE_8] = {0x09, '8', '*', 0},
    [SDL_SCANCODE_9] = {0x0a, '9', '(', 0},
    [SDL_SCANCODE_0] = {0x0b, '0', ')', 0},
    [SDL_SCANCODE_MINUS] = {0x0c, '-', '_', 31},
    [SDL_SCANCODE_EQUALS] = {0x0d, '=', '+', 0},
    [SDL_SCANCODE_BACKSPACE] = {0x0e, 8, 8, 127},
    [SDL_SCANCODE_TAB] = {0x0f, 9, 0, 0},
    [SDL_SCANCODE_Q] = {0x10, 'q', 'Q', 17},
    [SDL_SCANCODE_W] = {0x11, 'w', 'W', 23},
    [SDL_SCANCODE_E] = {0x12, 'e', 'E', 5},
    [SDL_SCANCODE_R] = {0x13, 'r', 'R', 18},
    [SDL_SCANCODE_T] = {0x14, 't', 'T', 20},
    [SDL_SCANCODE_Y] = {0x15, 'y', 'Y', 25},
    [SDL_SCANCODE_U] = {0x16, 'u', 'U', 21},
    [SDL_SCANCODE_I] = {0x17, 'i', 'I', 9},
    [SDL_SCANCODE_O] = {0x18, 'o', 'O', 15},
    [SDL_SCANCODE_P] = {0x19, 'p', 'P', 16},
    [SDL_SCANCODE_LEFTBRACKET] = {0x1a, '[', '{', 27},
    [SDL_SCANCODE_RIGHTBRACKET] = {0x1b, ']', '}', 29},
    [SDL_SCANCODE_RETURN] = {0x1c, 13, 13, 10},
    [SDL_SCANCODE_A] = {0x1e, 'a', 'A', 1},
    [SDL_SCANCODE_S] = {0x1f, 's', 'S', 19},
    [SDL_SCANCODE_D] = {0x20, 'd', 'D', 4},
    [SDL_SCANCODE_F] = {0x21, 'f', 'F', 6},
    [SDL_SCANCODE_G] = {0x22, 'g', 'G', 7},
    [SDL_SCANCODE_H] = {0x23, 'h', 'H', 8},
    [SDL_SCANCODE_J] = {0x24, 'j', 'J', 10},
    [SDL_SCANCODE_K] = {0x25, 'k', 'K', 11},
    [SDL_SCANCODE_L] = {0x26, 'l', 'L', 12},
    [SDL_SCANCODE_SEMICOLON] = {0x27, ';', ':', 0},
    [SDL_SCANCODE_APOSTROPHE] = {0x28, '\'', '"', 0},
    [SDL_SCANCODE_GRAVE] = {0x29, '`', '~', 0},
    [SDL_SCANCODE_BACKSLASH] = {0x2b, '\\', '|', 28},
    [SDL_SCANCODE_Z] = {0x2c, 'z', 'Z', 26},
    [SDL_SCANCODE_X] = {0x2d, 'x', 'X', 24},
    [SDL_SCANCODE_C] = {0x2e, 'c', 'C', 3},
    [SDL_SCANCODE_V] = {0x2f, 'v', 'V', 22},
    [SDL_SCANCODE_B] = {0x30, 'b', 'B', 2},
    [SDL_SCANCODE_N] = {0x31, 'n', 'N', 14},
    [SDL_SCANCODE_M] = {0x32, 'm', 'M', 13},
    [SDL_SCANCODE_COMMA] = {0x33, ',', '<', 0},
    [SDL_SCANCODE_PERIOD] = {0x34, '.', '>', 0},
    [SDL_SCANCODE_SLASH] = {0x35, '/', '?', 0},
    [SDL_SCANCODE_KP_MULTIPLY] = {0x37, '*', '*', 0},
    [SDL_SCANCODE_SPACE] = {0x39, ' ', ' ', ' '},
    [SDL_SCANCODE_F1] = {0x3b, 0, 0, 0},
    [SDL_SCANCODE_F2] = {0x3c, 0, 0, 0},
    [SDL_SCANCODE_F3] = {0x3d, 0, 0, 0},
    [SDL_SCANCODE_F4] = {0x3e, 0, 0, 0},
    [SDL_SCANCODE_F5] = {0x3f, 0, 0, 0},
    [SDL_SCANCODE_F6] = {0x40, 0, 0, 0},
    [SDL_SCANCODE_F7] = {0x41, 0, 0, 0},
    [SDL_SCANCODE_F8] = {0x42, 0, 0, 0},
    [SDL_SCANCODE_F9] = {0x43, 0, 0, 0},
    [SDL_SCANCODE_F10] = {0x44, 0, 0, 0},
    [SDL_SCANCODE_HOME] = {0x47, 0, 0, 0},
    [SDL_SCANCODE_UP] = {0x48, 0, 0, 0},
    [SDL_SCANCODE_PAGEUP] = {0x49, 0, 0, 0},
    [SDL_SCANCODE_KP_MINUS] = {0x4a, '-', '-', 0},
    [SDL_SCANCODE_LEFT] = {0x4b, 0, 0, 0},
    [SDL_SCANCODE_RIGHT] = {0x4d, 0, 0, 0},
    [SDL_SCANCODE_KP_PLUS] = {0x4e, '+', '+', 0},
    [SDL_SCANCODE_END] = {0x4f, 0, 0, 0},
    [SDL_SCANCODE_DOWN] = {0x50, 0, 0, 0},
    [SDL_SCANCODE_PAGEDOWN] = {0x51, 0, 0, 0},
    [SDL_SCANCODE_INSERT] = {0x52, 0, 0, 0},
    [SDL_SCANCODE_DELETE] = {0x53, 0, 0, 0},
    [SDL_SCANCODE_KP_ENTER] = {0x1c, 13, 13, 10},
    [SDL_SCANCODE_KP_DIVIDE] = {0x35, '/', '/', 0},
};

/* The keypad's digit keys: with Num Lock off they are the cursor keys. */
static const struct { SDL_Scancode sdl; uint8_t scan; char digit; } keypad[] = {
    {SDL_SCANCODE_KP_7, 0x47, '7'}, {SDL_SCANCODE_KP_8, 0x48, '8'},
    {SDL_SCANCODE_KP_9, 0x49, '9'}, {SDL_SCANCODE_KP_4, 0x4b, '4'},
    {SDL_SCANCODE_KP_5, 0x4c, '5'}, {SDL_SCANCODE_KP_6, 0x4d, '6'},
    {SDL_SCANCODE_KP_1, 0x4f, '1'}, {SDL_SCANCODE_KP_2, 0x50, '2'},
    {SDL_SCANCODE_KP_3, 0x51, '3'}, {SDL_SCANCODE_KP_0, 0x52, '0'},
    {SDL_SCANCODE_KP_PERIOD, 0x53, '.'},
};

/* The BIOS word for a key press, or -1 for a key the BIOS buffers nothing
   for (shift keys and the like). */
static int32_t bios_word(SDL_Scancode sc, SDL_Keymod mod)
{
    int shift = (mod & SDL_KMOD_SHIFT) != 0;
    int ctrl = (mod & SDL_KMOD_CTRL) != 0;
    int alt = (mod & SDL_KMOD_ALT) != 0;

    for (size_t i = 0; i < sizeof keypad / sizeof keypad[0]; i++) {
        if (keypad[i].sdl != sc)
            continue;
        if (((mod & SDL_KMOD_NUM) != 0) != shift && !ctrl && !alt)
            return keypad[i].scan << 8 | (uint8_t)keypad[i].digit;
        if (keypad[i].scan == 0x4c)
            return -1;
        return keypad[i].scan << 8;
    }
    if (sc <= SDL_SCANCODE_UNKNOWN || sc >= SDL_SCANCODE_COUNT || !keys[sc].scan)
        return -1;
    const struct keydef *k = &keys[sc];
    if (k->scan >= 0x3b && k->scan <= 0x44) {       /* F1..F10 */
        int base = alt ? 0x68 : ctrl ? 0x5e : shift ? 0x54 : 0x3b;
        return (base + k->scan - 0x3b) << 8;
    }
    if (alt) {
        if (k->lo >= 'a' && k->lo <= 'z')
            return k->scan << 8;
        if (k->lo >= '1' && k->lo <= '9')
            return (k->scan + 0x76) << 8;
        if (k->lo == '0')
            return 0x81 << 8;
        return -1;
    }
    if (ctrl)
        return k->ctl ? k->scan << 8 | k->ctl : -1;
    uint8_t c = shift ? k->up : k->lo;
    if (k->lo >= 'a' && k->lo <= 'z' && (mod & SDL_KMOD_CAPS))
        c = shift ? k->lo : k->up;
    if (!k->lo)
        return k->scan << 8;        /* an extended key */
    if (!c)
        return k->scan << 8;        /* Shift+Tab */
    return k->scan << 8 | c;
}

void bios_key_event(const SDL_KeyboardEvent *e)
{
    int32_t w = bios_word(e->scancode, e->mod);
    if (w < 0)
        return;
    SDL_LockMutex(lock);
    if (key_count < KEYBUF) {       /* full: the BIOS beeps and drops it */
        keybuf[(key_head + key_count) % KEYBUF] = (uint16_t)w;
        key_count++;
        SDL_SignalCondition(key_arrived);
    }
    SDL_UnlockMutex(lock);
}

/* ---- mouse ------------------------------------------------------------------ */

static struct {
    int present;
    uint16_t x, y, buttons;
    uint16_t min_x, max_x, min_y, max_y;
    uint16_t mask;
    mouse_handler_fn handler;
    const uint8_t *image;
    uint16_t w, h, stride;
    int shown;
    int warp;
} mouse = { .max_x = 639, .max_y = 199 };

static uint16_t clamp16(int v, int lo, int hi)
{
    return (uint16_t)(v < lo ? lo : v > hi ? hi : v);
}

uint16_t mouse_reset(void)
{
    SDL_LockMutex(lock);
    mouse.present = 1;
    mouse.min_x = 0;
    mouse.max_x = 639;
    mouse.min_y = 0;
    mouse.max_y = 199;
    mouse.x = 320;
    mouse.y = 100;
    mouse.mask = 0;
    mouse.handler = 0;
    mouse.shown = 0;
    SDL_UnlockMutex(lock);
    return 2;
}

uint16_t mouse_get(uint16_t *x, uint16_t *y)
{
    SDL_LockMutex(lock);
    *x = mouse.x;
    *y = mouse.y;
    uint16_t b = mouse.buttons;
    SDL_UnlockMutex(lock);
    return b;
}

void mouse_move_to(uint16_t x, uint16_t y)
{
    SDL_LockMutex(lock);
    mouse.x = clamp16(x, mouse.min_x, mouse.max_x);
    mouse.y = clamp16(y, mouse.min_y, mouse.max_y);
    mouse.warp = 1;
    SDL_UnlockMutex(lock);
    platform_wake();
}

void mouse_set_range_x(uint16_t min, uint16_t max)
{
    SDL_LockMutex(lock);
    mouse.min_x = min < max ? min : max;
    mouse.max_x = min < max ? max : min;
    mouse.x = clamp16(mouse.x, mouse.min_x, mouse.max_x);
    SDL_UnlockMutex(lock);
}

void mouse_set_range_y(uint16_t min, uint16_t max)
{
    SDL_LockMutex(lock);
    mouse.min_y = min < max ? min : max;
    mouse.max_y = min < max ? max : min;
    mouse.y = clamp16(mouse.y, mouse.min_y, mouse.max_y);
    SDL_UnlockMutex(lock);
}

void mouse_set_handler(uint16_t mask, mouse_handler_fn handler)
{
    SDL_LockMutex(lock);
    mouse.mask = handler ? mask : 0;
    mouse.handler = handler;
    SDL_UnlockMutex(lock);
}

void mouse_pointer_image(const uint8_t *image, uint16_t w, uint16_t h, uint16_t stride)
{
    SDL_LockMutex(lock);
    mouse.image = image;
    mouse.w = w;
    mouse.h = h;
    mouse.stride = stride;
    SDL_UnlockMutex(lock);
}

void mouse_pointer_show(int on)
{
    SDL_LockMutex(lock);
    mouse.shown = on;
    SDL_UnlockMutex(lock);
}

/* Call the handler, outside the lock, for the events that match. */
static void notify(uint16_t events)
{
    SDL_LockMutex(lock);
    mouse_handler_fn fn = mouse.present ? mouse.handler : 0;
    uint16_t ev = events & mouse.mask;
    uint16_t b = mouse.buttons, x = mouse.x, y = mouse.y;
    SDL_UnlockMutex(lock);
    if (fn && ev)
        fn(ev, b, x, y);
}

void bios_mouse_motion(float x, float y)
{
    SDL_LockMutex(lock);
    uint16_t nx = clamp16((int)(x * 2), mouse.min_x, mouse.max_x);
    uint16_t ny = clamp16((int)y, mouse.min_y, mouse.max_y);
    int moved = nx != mouse.x || ny != mouse.y;
    mouse.x = nx;
    mouse.y = ny;
    SDL_UnlockMutex(lock);
    if (moved)
        notify(1);
}

void bios_mouse_button(uint8_t sdl_button, int down)
{
    uint16_t bit, ev;
    switch (sdl_button) {
    case SDL_BUTTON_LEFT: bit = 1; ev = down ? 2 : 4; break;
    case SDL_BUTTON_RIGHT: bit = 2; ev = down ? 8 : 16; break;
    case SDL_BUTTON_MIDDLE: bit = 4; ev = down ? 32 : 64; break;
    default: return;
    }
    SDL_LockMutex(lock);
    if (down)
        mouse.buttons |= bit;
    else
        mouse.buttons &= (uint16_t)~bit;
    SDL_UnlockMutex(lock);
    notify(ev);
}

int bios_pointer(struct pointer_state *p)
{
    int ok;
    SDL_LockMutex(lock);
    ok = mouse.present && mouse.shown && mouse.image && mouse.w && mouse.h;
    if (ok) {
        p->image = mouse.image;
        p->w = mouse.w;
        p->h = mouse.h;
        p->stride = mouse.stride;
        p->x = mouse.x / 2;
        p->y = mouse.y;
    }
    SDL_UnlockMutex(lock);
    return ok;
}

int bios_take_warp(float *x, float *y)
{
    int w;
    SDL_LockMutex(lock);
    w = mouse.warp;
    mouse.warp = 0;
    *x = mouse.x / 2 + 0.5f;
    *y = mouse.y + 0.5f;
    SDL_UnlockMutex(lock);
    return w;
}

/* ---- VGA -------------------------------------------------------------------- */

static uint8_t video_mode = 3;

void vga_set_mode(uint8_t mode)
{
    video_mode = mode & 0x7f;
    if (!(mode & 0x80))
        memset(DOS_PTR(VGA_SEGMENT, 0), 0, 0x10000);
    if (video_mode == 0x13) {
        /* The default VGA palette's first 16 entries; the game sets its own. */
        static const uint8_t ega[16][3] = {
            {0, 0, 0}, {0, 0, 42}, {0, 42, 0}, {0, 42, 42}, {42, 0, 0}, {42, 0, 42},
            {42, 21, 0}, {42, 42, 42}, {21, 21, 21}, {21, 21, 63}, {21, 63, 21},
            {21, 63, 63}, {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63},
        };
        memset(vga_dac, 0, sizeof vga_dac);
        memcpy(vga_dac, ega, sizeof ega);
    }
}

uint8_t vga_get_mode(void)
{
    return video_mode;
}

void vga_wait_retrace(void)
{
    const uint64_t period = 1000000000ull / 70;
    uint64_t now = SDL_GetTicksNS();
    uint64_t next = (now / period + 1) * period;
    SDL_DelayPrecise(next - now);
}

/* ---- startup ---------------------------------------------------------------- */

void bios_init(void)
{
    lock = SDL_CreateMutex();
    key_arrived = SDL_CreateCondition();

    /* The BIOS counts ticks since midnight; start from the time of day, so
       seed_random_from_timer differs from run to run. */
    SDL_Time now;
    SDL_DateTime dt;
    start_ticks = 0;
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true))
        start_ticks = (int32_t)(((int64_t)dt.hour * 3600 + dt.minute * 60 + dt.second) *
                                1193182 / 65536);
    start_ns = SDL_GetTicksNS();
    update_ticks();
    SDL_AddTimerNS(2000000, tick_timer, 0);
}
