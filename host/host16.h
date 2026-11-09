/* Forced include for every game C file (tools/build.py): what the Aztec C86
   sources need on the host.  This header is not rewritten by c16.py. */
#ifndef HOST16_H
#define HOST16_H

/* 8086 pointer kinds mean nothing on the host. */
#define far
#define near
#define huge

/* Aztec laid out structs with no padding past 2-byte alignment, and the
   game's structs are read from its data files and laid over DOS memory:
   tools/check_structs.py checks them against their DOS offsets.  Host
   headers that define structs (overlay.h) put the default back around
   them. */
#pragma pack(1)

/* The game's main runs on its own thread, called by the host's main. */
#define main ds_main

/* Game names that are also C library names: renamed, so the game's own
   version is never mistaken for libc's (or libc's for the game's).  Game C
   that wants the C library's own function includes the system header and
   uses a name not listed here. */
#define errno aztec_errno

#endif
