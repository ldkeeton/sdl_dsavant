/* The emulated real-mode memory the drivers address by segment:offset.

   dos_mem is the 8086's whole 1 MB address space (plus the 64 KB past it
   that seg:off can reach), so a seg:off pair is just dos_mem + seg*16 + off:
     0000:0000  interrupt vectors (unused)
     0040:0000  BIOS data area; the tick counter is at 0040:006C
     DOS_ARENA_FIRST..DOS_ARENA_END  blocks for int 21h/48h, 49h, 4Ah
     A000:0000  the VGA framebuffer (mode 13h, 320x200, one byte a pixel)

   The game's own code and data are host memory, not in here; only what the
   drivers kept in DOS blocks is.

   EMS (int 67h) is a separate flat array: a handle's logical pages are
   consecutive in it, so a driver can address page n of a handle without
   mapping it into a page frame. */
#ifndef DOSMEM_H
#define DOSMEM_H

#include <stdint.h>

#define DOS_MEM_SIZE    0x110000
#define DOS_ARENA_FIRST 0x1800      /* first paragraph DOS can hand out */
#define DOS_ARENA_END   0xA000      /* end of conventional memory */
#define VGA_SEGMENT     0xA000

extern uint8_t dos_mem[DOS_MEM_SIZE];

/* The host address of seg:off. */
#define DOS_PTR(seg, off) \
    (dos_mem + (((uint32_t)(uint16_t)(seg) << 4) + (uint16_t)(off)))
/* The segment of a paragraph-aligned host address inside dos_mem. */
#define DOS_SEG(p) ((uint16_t)(((const uint8_t *)(p) - dos_mem) >> 4))

/* DOS error codes, as int 21h returns them in AX with carry set. */
#define DOSERR_FILE_NOT_FOUND   2
#define DOSERR_PATH_NOT_FOUND   3
#define DOSERR_TOO_MANY_OPEN    4
#define DOSERR_ACCESS_DENIED    5
#define DOSERR_INVALID_HANDLE   6
#define DOSERR_MCB_DESTROYED    7
#define DOSERR_NO_MEMORY        8
#define DOSERR_INVALID_BLOCK    9

/* int 21h/48h: allocate paras paragraphs.  Returns the segment, or
   -DOSERR_NO_MEMORY with *largest set to the largest block free (so
   paras = 0xFFFF asks how much there is). */
int32_t dos_alloc(uint16_t paras, uint16_t *largest);
/* int 21h/49h: free the block at seg.  0, or -DOSERR_INVALID_BLOCK. */
int32_t dos_free(uint16_t seg);
/* int 21h/4Ah: resize the block at seg.  0, or -DOS error code with
   *largest set to the most the block could grow to. */
int32_t dos_resize(uint16_t seg, uint16_t paras, uint16_t *largest);

/* EMS: 16 KB pages. */
#define EMS_PAGE_SIZE   0x4000
#define EMS_PAGES       24          /* 384 KB */

extern uint8_t ems_mem[EMS_PAGES * EMS_PAGE_SIZE];

/* int 67h/42h: total and free pages. */
void ems_page_counts(uint16_t *total, uint16_t *free_pages);
/* int 67h/43h: allocate pages; returns a handle (>= 1), or -1. */
int32_t ems_alloc(uint16_t pages);
/* int 67h/45h: release a handle; 0, or -1. */
int32_t ems_free(uint16_t handle);
/* The host address of logical page `page` of a handle, or 0. */
uint8_t *ems_page(uint16_t handle, uint16_t page);

/* Clear dos_mem and the block lists (platform startup). */
void dosmem_init(void);

#endif
