# SDL port of Wizardry VII: Crusaders of the Dark Savant

Native Linux and Windows port with SDL3 graphics, keyboard and mouse input,
music, and sampled sound.

Place the original game's files in `DSAVANT` at the repository root. The build
creates that folder if needed and puts both executables and `SDL3.dll` there.

## Build

Requires Python 3, GCC/G++ with C++17 support, GNU binutils, `pkg-config`,
SDL3 development libraries, the MinGW-w64 GCC/G++ toolchain, and SDL3 for
MinGW. SDL3 for MinGW is expected under `/usr/local/x86_64-w64-mingw32`; set
`MINGW_SDL3_PREFIX` if installed elsewhere.

Build both Linux and Windows versions from the repository root:

```sh
./build.sh
```

Game files and saves in `DSAVANT` are preserved across builds. Launch the
Linux build with:

```sh
cd DSAVANT
./DS
```

On Windows, launch `DS_WIN.exe` from the same folder.

## Controls

- **Alt+Enter:** toggle fullscreen.
- **F12:** save a screenshot.
