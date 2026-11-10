# SDL port of Wizardry VII: Crusaders of the Dark Savant

Native Linux and Windows port with SDL3 graphics, keyboard and mouse input,
music, and sampled sound.

Place the original game's files in `DSAVANT` at the repository root. The build
creates that folder if needed and puts both executables and `SDL3.dll` there.

## Build

The Linux build requires Python 3, GCC/G++ with C++17 support, GNU binutils,
`pkg-config`, and SDL3 development libraries. The Windows build also requires
MinGW-w64 GCC/G++ and SDL3 for MinGW. SDL3 for MinGW is expected under
`/usr/local/x86_64-w64-mingw32`; set `MINGW_SDL3_PREFIX` if installed elsewhere.

Build Linux from the repository root. Build Windows only when requested:

```sh
./build.sh
TARGET=win ./build.sh
```

Game files and saves in `DSAVANT` are preserved across builds. Launch the
Linux build from the repository root with:

```sh
./run.sh
```

On Windows, launch `DS_WIN.exe` from the same folder.

## Releases

Pushing a tag beginning with `v` runs the GitHub Actions release build:

```sh
git tag v0.1.0
git push origin v0.1.0
```

The release contains the Linux `DS` binary and `DS_WIN.zip` with
`DS_WIN.exe` and `SDL3.dll`. Original game data is not included.

## Controls

- **Alt+Enter:** toggle fullscreen.
- **F12:** save a screenshot.

## Battle auto mode

Turn on **BATTLE AUTO MODE** in Game Configuration to enable **AUTO** battle
orders: Fight the nearest group when possible, otherwise Guard. It also
speeds through the round.
