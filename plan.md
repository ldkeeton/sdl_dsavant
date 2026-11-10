# Plan: host (SDL) build of src_DSAVANT

Goal: build and run the game natively on Linux and Windows with SDL3,
replacing every 8086 `.s` module with C, and changing the existing game C only
where it cannot work on a 64-bit host.

Decided:

- **Platforms:** Linux (native gcc) and Windows (cross-compiled from Linux with
  mingw-w64). Both are 64-bit.
- **SDL3**
- **`.s` files:** each one is deleted as soon as its `.c` replacement lands.
  The DOS build is not kept; git history has it.
- **`build.sh`/`run.sh`:** new host scripts at the repo root, run from the
  root. `src_DSAVANT/build.sh` and `src_DSAVANT/run.sh` (DOSBox-X/Aztec) are
  removed.
- **OPL emulator:** ymfm (BSD-3), vendored.

## Current status

The native SDL3 port is playable on Linux, with music and sound effects
confirmed by user testing. All assembly replacements are in C; the final
56 PLAY assembly files have been removed, including their data tables.
Both Linux and Windows builds link without stubs (`STUBS=0`), and Windows
smoke and regression checks pass under Wine. Remaining work: test on real
Windows, update `readme.md`, and continue broader gameplay testing. A full
playthrough has not been verified.

## Original DOS source inventory

| Part | Files | Role |
|---|---|---|
| `ds/` | 86 C, 70 asm | main loop, overlay loader, hardware init, keyboard, fonts, RNG, bit helpers, driver thunks |
| `shared/` | 123 C, 93 asm | Aztec C runtime (asm + C), the shared video-driver code (`shared/drv`), shared overlay C (`shared/ovr`) |
| 15 overlays (`vbase` … `vtrea`) | ~700 C, 0 asm | game logic, loaded one at a time over a fixed memory region |
| `vga/` | 24 asm | VGA-specific driver primitives (mouse pointer, palette, fades, dissolve) |
| `play/` | 56 asm | sound driver: OPL (AdLib) / MPU-401 music, digitized samples on the PIT |

How the parts call each other: game C calls `call_*` thunks, which far-call
fixed slots in the loaded `VGA.DRV`. The driver owns all memory for
pictures, mazes, screens and sounds. Those memory blocks are addressed by
paragraph, both in a 268 KB "MAZE" area and in EMS. The game gets them back
only as opaque 32-bit handles (`long saved_area`, `screen_cache[]`), which it
never dereferences. Sound and music go from the video driver to `PLAY.DRV`
far entries.

A trial compile of all the game C with host gcc gives only 13 hard errors:
the code is clean K&R with no floats. Struct offsets are documented in
`include/structs/*.h`.

## Key decisions

### 1. Keep Aztec's integer sizes: `int` = 16 bits, `long` = 32 bits

Data files, save games, raw `memmove`s of global arrays and the struct layouts
all assume these sizes. Changing every type by hand would be a huge, risky diff.
Instead the host build adds one step after preprocessing (`tools/c16.py`):

- `gcc -E` → rewrite tokens → `gcc -c -x cpp-output`
- `int`→`short`, `long`→`int`, a bare `unsigned`/`signed`→`… short`
- implicit-`int` function declarations and definitions get an explicit `short`,
  so declarations and definitions agree
- an `L` suffix on a number is dropped, and `(unsigned)N` for N in
  32768..65535 becomes `((short)N)`: the source writes 16-bit patterns such
  as 0xff01 this way and compares them with `int`s
- code from system headers and `host/` headers is left alone (detected from
  cpp line markers)

Every game C file also gets a generated prototype for each external function
the game C defines, with K&R parameter types promoted. Calls are then checked
across files, which is how pointers passed as `long` (64-bit truncation)
show up as compile errors. A file that declares a function itself keeps its
own return type (`void f()` for an implicit-int definition is harmless).
Functions called with fewer or more arguments than they take, as the DOS
code does in a few places, are listed in `UNCHECKED` in `tools/build.py`.
A by-value struct parameter is not checked either (the code passes
look-alike structs with other tags).

Game C, the `.s` replacements included, cannot include system headers: its
include path holds Aztec's `stdio.h`, `errno.h` …, which the system headers
would pick up. It calls the C library through `host/hostlib.h`.

The sources stay as they are, and the DOS semantics (16-bit wraparound, sizes
of structs) are kept. Pointers become 64-bit. Wherever the code stores a
pointer in an `int` or `long`, it must be fixed by hand; gcc 15 makes most of
these errors (`-Wint-conversion`).

Other game-C compile settings:

- `-O0` (busy-waits such as `while (*tick_counter_ptr == t);` must re-read memory)
- `-fno-builtin`, `-fno-common`, `-fno-toplevel-reorder`, so globals stay in
  definition order. `globals.c` relies on that order.
- `-Dfar= -Dnear= -Dhuge=`, and the game's `main` is renamed to `ds_main`
- game names that clash with libc (`errno`; later e.g. `strstr`, which
  here returns an offset) are renamed in `host/host16.h`. Otherwise the
  exe's copy would interpose libc's for SDL too.

### 2. Asm replacements are C in the same place, compiled as "16-bit C"

`foo.s` becomes `foo.c` in the same directory. These files go through the
same rewrite, so `int *` arguments match the game's `short *`, and they can
use `<stdint.h>`, `<string.h>` and `host/*.h`. Two styles:

- simple routines are rewritten as plain idiomatic C
- complex ones (`render_text_glyph_by_mode`, `draw_text`, `draw_maze_piece`,
  `draw_glyph`, maze/EMS compaction, `music_player_tick` …) are transliterated
  register by register (`uint16_t ax, bx, si, di`, segment base pointers).
  This keeps every quirk, including bugs the game may depend on.

The Aztec runtime (`aztec_*`) is reimplemented on top of the host:
file I/O, strings, `sprintf`/`format`, malloc and the far-heap calls. The asm
that only supports the compiler goes away, because nothing calls it by name:
`csav`, `lsubs`, `cswt`, `fltinit`, `rsvstk`, `begin`.

### 3. Emulated DOS memory inside the driver

The driver's `seg:off` logic is kept by giving it byte arrays:

- screen buffer: 64000 bytes
- MAZE area: `0x4180` paragraphs
- scratch areas
- 384 KB of EMS, as one flat array (consecutive logical pages are contiguous,
  so mapping pages becomes a no-op)
- a small "DOS allocator" (`host/dosmem.c`) for `int 21h/48h` blocks
  (`save_area`, fonts, fades)

Handles are fake 16-bit segments, so `long` handles fit in 32 bits exactly as
before. `get_font_glyph` returns a real host pointer, because game C
dereferences it.

### 4. Overlays are linked into the exe, each in its own namespace

Windows DLLs can't resolve symbols back into the exe the way a `dlopen`'d `.so`
can. So both platforms use one approach, with no `dlopen`:

- each overlay's objects, plus the `libshared` members DS does not already
  link (this is what `ln` did), are combined with `ld -r` into a single object,
  `VMAZE.o`
- `objcopy` makes every symbol defined in that object local, except `ovmain`,
  which becomes `ovmain_VMAZE`. Duplicate names across overlays then no longer
  clash. Undefined symbols stay global, so they resolve to DS.
- the overlay's `.data`/`.bss` are renamed to per-overlay sections, with
  bounds symbols. At startup the loader snapshots each overlay's initial
  data. `aztec_ovloader("VMAZE")` restores the snapshot, zeroes the BSS and
  calls `ovmain_VMAZE` through a generated table. As on DOS, every load starts
  with fresh data.

This gives one `DS`/`DS.exe` and no overlay files.

### 5. The drivers are linked into the exe

The `call_*` thunks become direct C calls into the VGA driver (`shared/drv` +
`vga/`) and the PLAY driver. Any name clashes get a `drv_` prefix. The driver
header and block table that `load_video_driver` copied are replaced by plain
shared globals, set by the host `load_video_driver`/`load_play_driver`.

### 6. Threads: the game thread, and SDL as the "hardware"

- **Game thread:** runs `ds_main` and the overlays, unchanged, including their
  busy-wait loops.
- **Main thread (SDL):** handles events and draws at 60 Hz. It plays the part
  of the DOS hardware:
  - timer: a BIOS tick counter at 18.2065 Hz, which `tick_counter_ptr` points at
  - keyboard: a BIOS-style key buffer (`int 16h`/DOS 07h); arrow keys map to
    the extended scan codes
  - mouse: `int 33h` state (position, buttons, the click callback that sets
    `mouse_click_pending`/`x`/`y`)
  - VGA: an "A000" framebuffer plus a 256-entry 6-bit DAC
- **Mouse pointer:** drawn by the presenter over the VGA framebuffer, with
  0xFF transparent. The driver's save-under, redraw and clip code turns into
  bookkeeping of the pointer's position and image. Glides move it through the
  emulated `int 33h` and warp the host cursor.
- **Palette and fades:** fades write the DAC once per step and wait about one
  retrace (1/70 s), the way the original waits on port 3DAh.
- **Delays:** `delay_ms` becomes a real sleep, and `calibrate_loops_per_ms`
  becomes a constant.
- **Window:** 320×200, scaled (integer scaling, optional 4:3 aspect).

### 7. Files and paths

DOS file calls map to host files in the game directory. Name lookup ignores
case. `C:\DSAVANT\…` and relative paths resolve to the game directory, and
text-mode CR/LF is left as is (the game uses binary I/O).

### 8. Sound

The original music player was in `play/*.s`: it parses the score, handles tempo
and volume, and writes registers to the AdLib OPL2 chip at port 388h. The chip
itself only synthesizes. Digitized samples are pushed out one byte per PIT
tick by the CPU, to the PC speaker, a parallel-port DAC or the OPL.

- **Phase A:** a `PLAY` stub that reports no device, so the game runs silent.
- **Phase B:** port `play/` (the OPL path) to C. `opl_write` feeds ymfm's
  `ym3812` (OPL2) core, which renders PCM into an SDL3 audio stream. ymfm is
  C++, so it sits in `third_party/ymfm/` behind a small C wrapper
  (`host/opl.cpp`, `extern "C"`), the only C++ in the build. The music timer
  interrupt becomes a tick driven by the audio stream, at the original rate.
  Digitized samples are mixed into the same stream as plain PCM. The PC
  speaker, LPT DAC and MPU-401 paths are dropped.

## Toolchains

| Target | Compiler | SDL3 | Output |
|---|---|---|---|
| Linux (default) | host `gcc` 15 | system `libsdl3-dev` (3.4.2), via `pkg-config sdl3` | `build_out/linux/DS` |
| Windows (`TARGET=win`) | `x86_64-w64-mingw32-gcc` 13 (Ubuntu `mingw-w64` package) | official SDL3 3.4.2 mingw release, installed in `/usr/local/x86_64-w64-mingw32`; `PKG_CONFIG_LIBDIR=/usr/local/x86_64-w64-mingw32/lib/pkgconfig` | `build_out/win/DS.exe` + `SDL3.dll` |

Both are installed, and a test SDL3 program cross-links to a PE32+ exe. Wine is
installed, so Windows builds can be smoke-tested here. `build.sh` reads
`MINGW_SDL3_PREFIX` if SDL3 is somewhere else.

Under both targets `int` is 32 bits (Windows is LLP64), so the `c16.py`
rewrite is the same. Host code avoids POSIX-only calls: threads, timing and
file lookup go through SDL3, or through a small `#ifdef _WIN32`.

## New files

Everything is run from the repo root (`./build.sh`, `./run.sh`). The
decompiled game and its C replacements for the `.s` files stay in
`src_DSAVANT/`. The new port code lives beside it:

```
build.sh                 host build (wraps tools/build.py),
                         TARGET=linux (default) or win
run.sh                   builds if needed, then runs build_out/<target>/DS
                         (the Windows build under Wine)
tools/
  build.py               the build: "every NAME/**.c" rule as before, cpp +
                         c16 + prototypes, library members, overlays, stubs
  c16.py                 post-cpp int16/long32 rewriter
  ovlink.py              ld -r + objcopy per overlay, plus the ovmain table
host/                    native C, not rewritten
  host16.h               forced include for game C (far/near, renames, ds_main)
  hostlib.c/.h           C library calls for game C
  stubs.c/.h             reports stubbed symbols (until every .s is replaced)
  platform.c/.h          SDL window, presenter, event loop, game thread, main()
  bios.c/.h              tick counter, key buffer, int 33h mouse, VGA DAC
  hoststd.h              <stddef.h>/<stdint.h> for game C, errno kept Aztec's
  dosmem.c/.h            the emulated 1 MB address space, DOS blocks, EMS
  dosfile.c/.h           DOS-style file API over the host filesystem
  selftest.c             DS_SELFTEST=1: exercises the platform without the game
  sound.c                SDL audio stream, PIT clock, sample mixing
  opl.cpp                C wrapper around ymfm
  overlay.c              overlay data snapshot/reset, the ovmain table
third_party/ymfm/        vendored ymfm (BSD-3), only the OPL files
src_DSAVANT/             game source; each .s is replaced by a .c in place
  include/dshw.h         DS's hardware state: driver blocks, fonts, EMS, mouse
  include/drv.h          the video driver's state, under its old names
  shared/drv/drv_*.c     the driver: memory and slots, loading, caching,
                         drawing, draw_text, mouse, sound and score calls
  vga/vga_*.c            the VGA side: show_area, palette and fades, pointer
  ds/library/call_thunks.c  call_* as plain calls into the drivers
apps/DSAVANT/            original game data (unchanged)
```

Output goes to `build_out/<target>/` (in `.gitignore`): `DS`/`DS.exe` and the
original data files, copied from `apps/DSAVANT` without overwriting saves
already there. Objects go under `build_out/<target>/obj/`.

## Phases

1. **Infrastructure** (done): `c16.py`, `host16.h`, the root
   `build.sh`/`run.sh`, `build.py`, `ovlink.py`; `src_DSAVANT/build.sh`/
   `run.sh` removed. All 821 game C files compile on both targets with
   int/pointer conversions as errors; the link stubs the 84 functions and 2
   data symbols still in `.s` (`STUBS=1`, the default), and both
   `build_out/linux/DS` and `build_out/win/DS.exe` link, with the overlay
   namespaces and data sections checked on ELF and PE. Fixes in the game C:
   hard-coded data-segment addresses (VMNPC's `""` literals,
   `&party_records[m].item_slots[g][s]`), struct assignment between
   look-alike tags, `app_malloc`/`app_free`/`app_memmove` and
   `bane_import_buffer` given pointer types, and the trial compile's syntax
   errors. Left for later phases: 248 calls still have no declaration (all to
   functions in `.s`; each gets a prototype when its C lands), and the
   numeric addresses passed to asm functions (e.g. `set_bitmap_bit(… +
   (unsigned)36116, …)` in `vdopt/trigger_item_cell_object.c`), which the
   prototypes will flag once those functions are C.
2. **Platform** (done): SDL window and presenter, BIOS timer, keyboard and
   mouse emulation, DOS file and memory emulation, the game thread.
   `dos_mem` is the whole real-mode address space, so a driver's `seg:off`
   is `DOS_PTR(seg, off)`: the framebuffer is at A000:0000 and the BIOS tick
   counter at 0040:006C (`tick_counter_ptr` now points there). DOS blocks
   come from paragraphs 1800h..A000h (544 KB); EMS is 24 pages, each
   handle's pages contiguous. The int 21h/16h/33h/10h calls are plain
   functions returning AX or -(DOS error) (`dosfile.h`, `bios.h`); the
   mouse handler runs on the SDL thread, like the interrupt it replaces.
   Debug aids: F12 saves `shotNNN.bmp`, `DS_SHOT=file.bmp,secs` saves a
   frame and quits, `DS_SELFTEST=1` runs a platform test in place of the
   game; it passes on Linux and under Wine.
3. **Runtime and DS library** (done): the Aztec runtime is C over the
   host (`aztec_io.c`, `aztec_impl_sys.c` and `aztec_isatty.c` over
   `dosfile.h`; the string and memory routines; `malloc` is the host's,
   because Aztec's free list kept pointers in 16-bit words; `format` is a
   printf engine over a `va_list`, and `sprintf` is variadic). The
   compiler-support asm (`csav`, `lsubs`, `cswt`, `begin`, `ovbgn` …) is
   gone. The DS library (keyboard queue, delays, RNG, bit helpers, fonts,
   `unpack_record`, `init_system_hardware` and its detectors, the
   driver-block and EMS bookkeeping) is C, with its shared state in
   `include/dshw.h`. Fixed in the game C: four more hard-coded addresses
   (`message_block_buffer` + 1024, `&party_records[pc].hand_attack[hand]`,
   `party_records[pc].spell_bitmap`, `party_region.state[current_level]`,
   the last one confirmed from the `[8D12h]` loads in VTREA.OVR). The host's
   `mouse_set_pos` is now `mouse_move_to` (the game has its own), and
   `host/hoststd.h` keeps mingw's `errno` macro out of game C. Both targets
   now get through `init_system_hardware` and stop at the first video
   driver stub (`call_get_free_space` returns 0, so VINIT exits with status
   4). The 50 stubs left are all driver-side; they move to phase 4.
4. **VGA driver** (done): shared/drv and vga/ are C (`drv_*.c`,
   `vga_*.c`, state in `include/drv.h`). The driver reads DS's block table,
   palette and EMS state directly (`maze_mem_seg` is `maze_area_seg`, and so
   on), so `load_video_driver`/`load_play_driver` are empty: the build does
   not ship the .DRV files. The `call_*` thunks are plain calls
   (`call_thunks.c`); `call_restore_screen` splits cache_area's handle back
   into slot and mode. Kept as on DOS: 16-bit offsets into the screen
   buffer, the slot/block numbering and compaction, the fatal messages.
   EMS is addressed flat (`EMS_PTR`), so page mapping is gone. The pointer
   is the host's: `mouse_event_handler` only records clicks, and show_area
   no longer takes the pointer off and puts it back. A DAC load waits 24 ms,
   the time DOS took feeding it a byte a scan line, so fades keep their
   speed. The PLAY entries (`play_select_device`, `music_*`,
   `wait_for_music`) are the 7 stubs left. Fixed in the game C:
   `select_character_portrait` takes save_area's handle as one `long` (DOS
   passed it as two words), `pick_up_inventory_entry` keeps it as a `long`
   and reaches the pixels through `DOS_PTR`, and `run_character_menu` ends
   its option list itself (on DOS the -1 was the next local on the stack;
   under Windows a garbage row appeared). Checked on Linux and under Wine:
   the intro, the title, the main menu, the character menu and character
   creation up to the race list. Load Savegame reaches the manual-word
   check. `DS_KEYS` types keys and clicks at set times for tests like
   these.
5. **First run** (done; broader gameplay testing continues): a saved game loads into the maze; walking,
   swimming, area messages and combat work on Linux. Fixed: structs are
   packed to 1 (`host16.h`; Aztec added no padding, and the savegame's
   `load_state` was 2 bytes off, so the party came back empty), checked
   against the DOS offsets by `tools/check_structs.py`; each
   `*/data/globals.c` is laid out as ln did, last declared first and
   byte-packed (`dos_commons` in build.py; confirmed against VMAZE.OVR's
   render-queue addresses); `tick_combatant` read an unset local as a party
   index (now the fighter's own). `DS_SHOT` takes several times.
   Chest loot fix: `item_or_loot_dialog` used a two-byte buffer for full
   captions ("DIVVY LOOT", "TAKE", "LEAVE"), overwriting its stack. It now
   has 40 bytes. `tools/check_loot_dialog.py [linux|win]` exercises every
   loot/item-use button with 1..6 rows; it reproduces the pre-fix Linux
   stack-smashing abort and passes after the fix.
6. **Sound** (done; user confirmed music and sound effects). ymfm is vendored in
   `third_party/ymfm/` (OPL, ADPCM and PCM files, unmodified). The active PLAY
   paths are C in `play/play_{music,opl,sample,tables}.c`, with shared state
   in `include/play.h`. `host/opl.cpp` wraps ymfm and `host/sound.c` drives
   the PIT handlers through an SDL3 audio stream, mixing PCM with a DC
   blocker. A silent clock thread keeps waits progressing without audio.
   Both targets link with `STUBS=0`. `tools/check_sound.py [linux|win]`
   checks events, voice allocation, OPL register values, fractional samples,
   waits, ramps, looping and all six real scores (nonzero synthesized PCM
   and stream completion); both pass, Windows under Wine. SDL dummy-audio
   smoke tests pass on both; the unavailable-audio fallback passes on Linux.
   All 397 struct layout checks still pass on both targets.
   All 56 original PLAY assembly files, including `play/data/*.s`, have
   been removed. Active routines and tables are consolidated into the four
   C files above; obsolete hardware, dispatch and unreachable routines are
   retired. The seven PLAY stubs are no longer needed.
   Findings from the original assembly, retained for reference:
   - Frames: argument i of a PLAY entry is at bp+8+2i. play_select_device
     (a0..a6): sample segment = ((a0+a3)>>4)+a1-1000h, offset =
     ((a2-a3)&~1)+10h, so it plays until the offset wraps to 0 (len = slot[1]);
     step: fraction += 0FFh-a4 each tick (+1 byte per carry); a5 volume (0 =
     stop sample *and* music, restore timer); a6 low byte flags (bit 0 wait
     for the end, bit 1 skip if a sample is playing; 0FFh = set device, high
     byte device; 4 becomes 1). The DAC port setup reads a4, not the port
     (a5): an original bug, harmless (devices 2/3 are PCM here).
   - The PIT check (pushf, bit 15) is 0 on a 286+, so play_sample_mode_flag
     is 0: speaker_sample_interrupt, sample_fraction_interrupt/tick and the
     tempo reprogramming in music_set_tempo_volume are dead. Also dead:
     every rhythm/special_note branch (cmp then unconditional jmp),
     loc_VKUV, opl_apply_volume, opl_load_operator_muted,
     music_timer_interrupt_divided, int 13h. opl_pitch_range is never set
     and no score has a 60h-6Fh pitch event (checked all six CSF files).
   - Timing: the sample handler runs every 72 PIT clocks (16572 Hz, exactly
     every 3 OPL samples at 49716 Hz, 24 PIT clocks each); tick_divider counts
     those down and calls music_player_tick, which reloads it with the tempo.
     Without a sample, the music handler runs at PIT divisor
     (tempo & 0FFh) * 48h (8-bit mul). The audio callback emulates PIT ch0
     and int 08h (divisor + handler pointer, under a recursive SDL mutex
     standing in for cli), with ymfm generating one sample at a time. A
     fallback thread runs the ticks if no audio device opens.
   - Sound Blaster/AdLib samples (play_device 1) went through the OPL:
     opl_silent_note_pulse keys ch 0 on, waits 2386 mode-3 counts (~1 ms, the
     phase reaches its peak), sets fnum 0 with the key still on; then each
     sample tick writes the curve (build_opl_sample_curve) to the latched
     address (40h) with ramps in/out (play_device_timer_flag 0FFh/0/1).
     The port keeps this through ymfm (advancing the chip ~1 ms in the
     pulse), including the OPL address latch (opl_write leaves 40h, the
     finish sets 3Fh). Devices 0/2/3 use the table level as PCM. A DC
     blocker removes the unipolar output's offset.
   - Player: events 00-0F note off, 10-1F note on (2 bytes: semitone-12 and
     (v>>2)+60h unless play_device 5), 20-2F skip 2, 30-4F channel patch, 50-5F
     skip 1, 60-6F pitch word, FE delay word (0: go on), FF or 70+ end (loop
     if loop_at_end). Voices: 11 records of 5 bytes (vel, semitone, note
     index, age word), 8 used, OPL channel = index+1. note_find_any jumps to
     note_write_voice without setting opl_channel_index (keep). velocity*2+1
     wraps in a byte in opl_write_operator_level (keep). opl_keys_off writes
     B1h..B9h on devices 1/4. Patches: 40h-byte header (patch per note
     index>>2), 11-byte records; scores are CSF + PBA (PBL for MPU, music
     device 5: parse it, send nothing).
7. **Windows** (build and Wine checks done; real Windows test pending):
   `TARGET=win` builds without stubs; SDL smoke tests, sound checks and
   treasure-dialog regressions pass under Wine. A run on real Windows is
   still needed.
8. **Cleanup** (PLAY assembly removal done): update `readme.md`.

## Known risks

- Pointers stored in `int`/`long`: e.g. `app_malloc`/`app_memmove` take far
  pointers as `long`. These get native C replacements with pointer types, and
  each other case is found by compiler errors.
- Code that relies on memory layout ("a loop runs from one array into the
  next"): each `*/data/globals.c` is laid out as Aztec's ln did (reversed,
  byte-packed; `dos_commons` in build.py), and structs are packed
  (`#pragma pack(1)` in host16.h, checked by `tools/check_structs.py`).
  Other initialized data keeps its order (`-fno-toplevel-reorder`), but gcc
  may pad between globals; check with the address map.
- Implicit-`int` callers of functions that return pointers: they truncate on
  64-bit. Found through `-Wint-conversion`; fixed by adding the `extern`
  declaration.
- Thread-safety between the game thread and the presenter: tearing is
  harmless, and the key and mouse queues use a lock.
