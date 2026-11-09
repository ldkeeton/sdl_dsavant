/* DOS file handles (int 21h/3Ch..42h, 41h) over host files in the game
   directory.  Each call returns what int 21h leaves in AX, or -(DOS error
   code) where DOS sets carry.

   Names are DOS names: a drive letter and directories are dropped, so
   "C:\DSAVANT\SAVEGAME.DBS" and "savegame.dbs" are both SAVEGAME.DBS in
   the game directory, and lookup ignores case.  Handles 0..4 are the DOS
   standard handles; 1 and 2 write to the host's stdout and stderr. */
#ifndef DOSFILE_H
#define DOSFILE_H

#include <stdint.h>

/* Set the game directory (platform startup); 0 means the current one. */
void dosfile_init(const char *dir);

/* 3Dh: mode is AL (0 read, 1 write, 2 read/write; sharing bits ignored). */
int32_t dos_open(const char *name, uint8_t mode);
/* 3Ch: create or truncate, open read/write. */
int32_t dos_create(const char *name, uint16_t attr);
/* 3Eh */
int32_t dos_close(uint16_t handle);
/* 3Fh / 40h: bytes moved. */
int32_t dos_read(uint16_t handle, void *buf, uint16_t count);
int32_t dos_write(uint16_t handle, const void *buf, uint16_t count);
/* 42h: whence is AL (0 start, 1 current, 2 end); the new position. */
int32_t dos_lseek(uint16_t handle, int32_t offset, uint8_t whence);
/* 41h */
int32_t dos_unlink(const char *name);
/* 44h/00h: device information word (bit 7: a character device). */
int32_t dos_ioctl_info(uint16_t handle);
/* 36h: free bytes on the game's drive, capped at 2 GB. */
int32_t dos_disk_free(void);

#endif
