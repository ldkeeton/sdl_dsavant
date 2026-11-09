/* Emulated DOS memory blocks and EMS (dosmem.h).  Called from the game
   thread only. */
#include <string.h>
#include "dosmem.h"

uint8_t dos_mem[DOS_MEM_SIZE];
uint8_t ems_mem[EMS_PAGES * EMS_PAGE_SIZE];

/* Allocated DOS blocks, sorted by segment. */
#define MAX_BLOCKS 256
static struct { uint16_t seg, paras; } blocks[MAX_BLOCKS];
static int nblocks;

/* EMS handles: a run of consecutive pages each. */
#define MAX_EMS_HANDLES 32
static struct { uint16_t first, pages; uint8_t used; } ems_handles[MAX_EMS_HANDLES];

void dosmem_init(void)
{
    memset(dos_mem, 0, sizeof dos_mem);
    memset(ems_mem, 0, sizeof ems_mem);
    nblocks = 0;
    memset(ems_handles, 0, sizeof ems_handles);
}

/* The free gap before blocks[i] (i == nblocks: the gap at the end). */
static void gap(int i, uint32_t *start, uint32_t *size)
{
    uint32_t lo = i == 0 ? DOS_ARENA_FIRST : (uint32_t)blocks[i - 1].seg + blocks[i - 1].paras;
    uint32_t hi = i == nblocks ? DOS_ARENA_END : blocks[i].seg;
    *start = lo;
    *size = hi > lo ? hi - lo : 0;
}

int32_t dos_alloc(uint16_t paras, uint16_t *largest)
{
    uint32_t best = 0;
    if (paras == 0)
        paras = 1;
    for (int i = 0; i <= nblocks; i++) {
        uint32_t start, size;
        gap(i, &start, &size);
        if (size >= paras && nblocks < MAX_BLOCKS) {
            memmove(&blocks[i + 1], &blocks[i], (size_t)(nblocks - i) * sizeof blocks[0]);
            blocks[i].seg = (uint16_t)start;
            blocks[i].paras = paras;
            nblocks++;
            return (int32_t)start;
        }
        if (size > best)
            best = size;
    }
    if (largest)
        *largest = (uint16_t)best;
    return -DOSERR_NO_MEMORY;
}

static int find_block(uint16_t seg)
{
    for (int i = 0; i < nblocks; i++)
        if (blocks[i].seg == seg)
            return i;
    return -1;
}

int32_t dos_free(uint16_t seg)
{
    int i = find_block(seg);
    if (i < 0)
        return -DOSERR_INVALID_BLOCK;
    memmove(&blocks[i], &blocks[i + 1], (size_t)(nblocks - i - 1) * sizeof blocks[0]);
    nblocks--;
    return 0;
}

int32_t dos_resize(uint16_t seg, uint16_t paras, uint16_t *largest)
{
    int i = find_block(seg);
    if (i < 0)
        return -DOSERR_INVALID_BLOCK;
    uint32_t limit = i + 1 < nblocks ? blocks[i + 1].seg : DOS_ARENA_END;
    uint32_t most = limit - seg;
    if (paras > most) {
        if (largest)
            *largest = (uint16_t)most;
        return -DOSERR_NO_MEMORY;
    }
    blocks[i].paras = paras;
    return 0;
}

static uint16_t ems_used_pages(void)
{
    uint16_t n = 0;
    for (int i = 0; i < MAX_EMS_HANDLES; i++)
        if (ems_handles[i].used)
            n += ems_handles[i].pages;
    return n;
}

void ems_page_counts(uint16_t *total, uint16_t *free_pages)
{
    *total = EMS_PAGES;
    *free_pages = (uint16_t)(EMS_PAGES - ems_used_pages());
}

int32_t ems_alloc(uint16_t pages)
{
    /* First fit over the page runs in use. */
    for (uint32_t first = 0; first + pages <= EMS_PAGES; first++) {
        int clash = 0;
        for (int i = 0; i < MAX_EMS_HANDLES && !clash; i++) {
            if (!ems_handles[i].used)
                continue;
            uint32_t a = ems_handles[i].first, b = a + ems_handles[i].pages;
            if (first < b && first + pages > a) {
                clash = 1;
                first = b - 1;
            }
        }
        if (clash)
            continue;
        for (int h = 1; h < MAX_EMS_HANDLES; h++) {
            if (!ems_handles[h].used) {
                ems_handles[h].used = 1;
                ems_handles[h].first = (uint16_t)first;
                ems_handles[h].pages = pages;
                return h;
            }
        }
        return -1;
    }
    return -1;
}

int32_t ems_free(uint16_t handle)
{
    if (handle == 0 || handle >= MAX_EMS_HANDLES || !ems_handles[handle].used)
        return -1;
    ems_handles[handle].used = 0;
    return 0;
}

uint8_t *ems_page(uint16_t handle, uint16_t page)
{
    if (handle == 0 || handle >= MAX_EMS_HANDLES || !ems_handles[handle].used ||
        page >= ems_handles[handle].pages)
        return 0;
    return ems_mem + (size_t)(ems_handles[handle].first + page) * EMS_PAGE_SIZE;
}
