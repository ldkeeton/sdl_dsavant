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

## What exists today

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

The whole music player is in `play/*.s`: it parses the score, handles tempo
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
  bios.c                 tick counter, key buffer, int 33h mouse, VGA DAC
  dosmem.c               emulated DOS blocks and EMS
  dosfile.c              DOS-style file API over the host filesystem
  audio.c                SDL audio stream, sample mixing
  opl.cpp                C wrapper around ymfm
  overlay.c              overlay data snapshot/reset, the ovmain table
third_party/ymfm/        vendored ymfm (BSD-3), only the OPL files
src_DSAVANT/             game source; each .s is replaced by a .c in place
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
2. **Platform:** SDL window and presenter, BIOS timer, keyboard and mouse
   emulation, DOS file and memory emulation, the game thread.
3. **Runtime and DS library:** `aztec_*`, overlay loader (data reset + ovmain table), keyboard
   queue (`get_filtered_key`/`poll_filtered_key`), fonts
   (`load_font_file`, `get_font_glyph`, `draw_font_glyph`,
   `measure_text_width`, `read_font_glyph`), RNG, bitfields,
   `unpack_record`/tree decoder, `init_system_hardware`, the `call_*` thunks.
4. **VGA driver:** memory and slot management (pictures, mazes, screens,
   sounds, scores, EMS caching, compaction), the blits (`show_area`,
   `fill_area`, `save_area`/`restore_area`, `cache_area`,
   `load_screen`/`save_screen`/`restore_screen`), `draw_glyph`, `draw_text`
   with `render_text_glyph_by_mode`, `draw_maze_piece`, palette
   (`set_rgb`/`get_rgb`/`fade_*`/`reset_rgb`/`set_palette`), `dissolve_area`,
   mouse.
5. **First run:** intro → main menu → load or create a party → maze → combat.
   Fix whatever the 64-bit host exposes: pointer truncation, global order,
   uninitialized reads.
6. **Sound:** port PLAY, vendor ymfm, samples.
7. **Windows:** `TARGET=win` build, smoke test under Wine, then a run on real
   Windows.
8. **Cleanup:** update `readme.md`, and add a struct-offset
   check that compares `offsetof` with the `/* +0x.. */` comments in
   `include/structs`.

## Known risks

- Pointers stored in `int`/`long`: e.g. `app_malloc`/`app_memmove` take far
  pointers as `long`. These get native C replacements with pointer types, and
  each other case is found by compiler errors.
- Code that relies on memory layout ("a loop runs from one array into the
  next"): global order is kept with `-fno-toplevel-reorder`. Padding may still
  differ; check with the address map.
- Implicit-`int` callers of functions that return pointers: they truncate on
  64-bit. Found through `-Wint-conversion`; fixed by adding the `extern`
  declaration.
- Thread-safety between the game thread and the presenter: tearing is
  harmless, and the key and mouse queues use a lock.
