# Plan: host (SDL) build of src_DSAVANT

Goal: build and run the game natively on the host (Linux first) with SDL,
replacing every 8086 `.s` module with C, and changing the existing game C only
where it cannot work on a 64-bit host.

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
- code from system headers and `host/` headers is left alone (detected from
  cpp line markers)

The sources stay as they are, and the DOS semantics (16-bit wraparound, sizes
of structs) are kept. Pointers become 64-bit. Wherever the code stores a
pointer in an `int` or `long`, it must be fixed by hand; gcc 15 makes most of
these errors (`-Wint-conversion`).

Other game-C compile settings:

- `-O0` (busy-waits such as `while (*tick_counter_ptr == t);` must re-read memory)
- `-fno-builtin`, `-fno-common`, `-fno-toplevel-reorder`, so globals stay in
  definition order. `globals.c` relies on that order.
- `-Dfar= -Dnear= -Dhuge=`, and the game's `main` is renamed to `ds_main`
- game functions whose names clash with libc (e.g. `strstr`, which here
  returns an offset) are renamed through a forced-include header. Otherwise
  the exe's copy would interpose libc's for SDL too.

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

### 4. Overlays are shared objects, loaded with `dlopen`

Each overlay becomes `VMAZE.so`, and so on. `aztec_ovloader("VMAZE")` calls
`dlclose` on the previous overlay, then `dlopen`s the new one and runs
`ovmain`. This copies the DOS behaviour exactly:

- initialized data and BSS are fresh on every load
- each overlay keeps its own namespace (`-Bsymbolic`), so duplicate function
  names across overlays do not clash
- overlays reach DS symbols through the exe (`-rdynamic`)

To match `ln` (an overlay uses DS's copy of any shared module DS already
links), each overlay is linked against `libshared.a` minus the members the exe
already contains.

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

- **Phase A:** a `PLAY` stub that reports no device, so the game runs silent.
- **Phase B:** port `play/` (the OPL path) to C and drive an OPL2 emulator.
  The emulator would be either a small one written here, or an
  MIT/BSD-licensed one vendored in (e.g. emu8950). The PIT sample interrupt
  becomes the SDL audio callback, ticking the music player at its original
  rate. Digitized samples are mixed into the same stream. MPU-401 output is
  optional, later.

## Scope questions (default in **bold**)

- **DOS build:** the `.s` files are deleted as their C replacements land (as
  asked), so `build.sh` stops working for DS/VGA/PLAY. Git history keeps the
  DOS version. Keeping both buildable would mean leaving each `.s` beside its
  `.c` (`build.sh` prefers `.s`), which is easy if you'd rather keep it.
- **SDL version:** **SDL2** (2.32 is installed; SDL3 3.4 is installed too and
  easy to switch to).

## New files

```
src_DSAVANT/
  build_host.sh          host build (same "every NAME/**.c" rule as build.sh)
  run_host.sh            runs build_out_host/DS with the game files
  tools/c16.py           post-cpp int16/long32 rewriter
  include/host16.h       forced include for game C (far/near, renames, ds_main)
  host/                  native C, not rewritten
    platform.c/.h        SDL window, presenter, event loop, game thread, main()
    bios.c               tick counter, key buffer, int 33h mouse, VGA DAC
    dosmem.c             emulated DOS blocks and EMS
    dosfile.c            DOS-style file API over the host filesystem
    audio.c              SDL audio stream (phase B: OPL emulation + samples)
```

Output goes to `build_out_host/`: `DS` (exe), `V*.so` overlays, and the
original data files copied from `../apps/DSAVANT`. Saves are kept there, as
`build.sh` does now.

## Phases

1. **Infrastructure:** `c16.py`, `host16.h`, `build_host.sh`; compile all game C
   with the rewrite, with temporary stubs for every asm symbol; fix the
   pointer/int errors in the C.
2. **Platform:** SDL window and presenter, BIOS timer, keyboard and mouse
   emulation, DOS file and memory emulation, the game thread.
3. **Runtime and DS library:** `aztec_*`, overlay loader (`dlopen`), keyboard
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
6. **Sound:** port PLAY, OPL emulation, samples.
7. **Cleanup:** remove `.s` files, update `readme.md`, and add a struct-offset
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
