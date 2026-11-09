/* DOS file handles over host files (dosfile.h).  Called from the game
   thread only. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "dosfile.h"
#include "dosmem.h"

#define MAX_HANDLES 20
#define FIRST_FILE  5

static FILE *files[MAX_HANDLES];
static char game_dir[1024];

void dosfile_init(const char *dir)
{
    snprintf(game_dir, sizeof game_dir, "%s", dir ? dir : "");
    size_t n = strlen(game_dir);
    if (n && game_dir[n - 1] != '/' && game_dir[n - 1] != '\\' && n + 1 < sizeof game_dir) {
        game_dir[n] = '/';
        game_dir[n + 1] = 0;
    }
}

/* The DOS file name without drive or directories. */
static const char *base_name(const char *name)
{
    const char *b = name;
    for (const char *p = name; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':')
            b = p + 1;
    return b;
}

struct match {
    const char *want;
    char found[256];
};

static SDL_EnumerationResult SDLCALL match_name(void *data, const char *dir, const char *name)
{
    struct match *m = data;
    (void)dir;
    if (SDL_strcasecmp(name, m->want) == 0) {
        snprintf(m->found, sizeof m->found, "%s", name);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

/* The host path of a DOS name: the existing file whose name matches
   ignoring case, else the name in upper case (for a new file). */
static void host_path(const char *name, char *out, size_t size)
{
    struct match m;
    char upper[256];
    size_t i;
    m.want = base_name(name);
    m.found[0] = 0;
    SDL_EnumerateDirectory(game_dir[0] ? game_dir : ".", match_name, &m);
    if (!m.found[0]) {
        for (i = 0; m.want[i] && i + 1 < sizeof upper; i++)
            upper[i] = (char)SDL_toupper((unsigned char)m.want[i]);
        upper[i] = 0;
        snprintf(m.found, sizeof m.found, "%s", upper);
    }
    snprintf(out, size, "%s%s", game_dir, m.found);
}

static int32_t error_code(void)
{
    switch (errno) {
    case ENOENT: return -DOSERR_FILE_NOT_FOUND;
    case ENOTDIR: return -DOSERR_PATH_NOT_FOUND;
    case EMFILE: case ENFILE: return -DOSERR_TOO_MANY_OPEN;
    default: return -DOSERR_ACCESS_DENIED;
    }
}

static int32_t new_handle(FILE *f)
{
    for (int h = FIRST_FILE; h < MAX_HANDLES; h++) {
        if (!files[h]) {
            files[h] = f;
            return h;
        }
    }
    fclose(f);
    return -DOSERR_TOO_MANY_OPEN;
}

static FILE *file_of(uint16_t handle)
{
    return handle >= FIRST_FILE && handle < MAX_HANDLES ? files[handle] : 0;
}

int32_t dos_open(const char *name, uint8_t mode)
{
    static const char *const modes[] = { "rb", "r+b", "r+b" };
    char path[1400];
    host_path(name, path, sizeof path);
    FILE *f = fopen(path, modes[(mode & 3) < 3 ? (mode & 3) : 0]);
    if (!f)
        return error_code();
    return new_handle(f);
}

int32_t dos_create(const char *name, uint16_t attr)
{
    char path[1400];
    (void)attr;
    host_path(name, path, sizeof path);
    FILE *f = fopen(path, "w+b");
    if (!f)
        return error_code();
    return new_handle(f);
}

int32_t dos_close(uint16_t handle)
{
    FILE *f = file_of(handle);
    if (handle < FIRST_FILE && handle < MAX_HANDLES)
        return 0;
    if (!f)
        return -DOSERR_INVALID_HANDLE;
    fclose(f);
    files[handle] = 0;
    return 0;
}

int32_t dos_read(uint16_t handle, void *buf, uint16_t count)
{
    FILE *f = file_of(handle);
    if (!f)
        return handle < FIRST_FILE ? 0 : -DOSERR_INVALID_HANDLE;
    /* A read right after a write must reposition (C stdio rule). */
    fseek(f, 0, SEEK_CUR);
    return (int32_t)fread(buf, 1, count, f);
}

int32_t dos_write(uint16_t handle, const void *buf, uint16_t count)
{
    if (handle == 1 || handle == 2) {
        fwrite(buf, 1, count, handle == 1 ? stdout : stderr);
        fflush(handle == 1 ? stdout : stderr);
        return count;
    }
    FILE *f = file_of(handle);
    if (!f)
        return handle < FIRST_FILE ? count : -DOSERR_INVALID_HANDLE;
    fseek(f, 0, SEEK_CUR);
    /* DOS truncates (or extends) the file at the position on a 0-byte
       write; nothing in the game relies on it. */
    return (int32_t)fwrite(buf, 1, count, f);
}

int32_t dos_lseek(uint16_t handle, int32_t offset, uint8_t whence)
{
    FILE *f = file_of(handle);
    if (!f)
        return -DOSERR_INVALID_HANDLE;
    if (fseek(f, offset, whence == 1 ? SEEK_CUR : whence == 2 ? SEEK_END : SEEK_SET) != 0)
        return -DOSERR_ACCESS_DENIED;
    return (int32_t)ftell(f);
}

int32_t dos_unlink(const char *name)
{
    char path[1400];
    host_path(name, path, sizeof path);
    if (remove(path) != 0)
        return error_code();
    return 0;
}

int32_t dos_ioctl_info(uint16_t handle)
{
    if (handle < FIRST_FILE)
        return handle == 0 || handle == 1 || handle == 2 ? 0x80d3 : 0x80c0;
    if (!file_of(handle))
        return -DOSERR_INVALID_HANDLE;
    return 0x0002;     /* a disk file on drive C */
}

int32_t dos_disk_free(void)
{
    return 0x7fffffff;
}
