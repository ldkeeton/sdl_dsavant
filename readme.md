# SDL port of Wizardry VII: Crusaders of the Dark Savant

Native Linux and Windows port with SDL3 graphics, keyboard and mouse input,
music, and sampled sound.

Place the original game's files in `DSAVANT` at the repository root.  

## Build
 
```sh
#Linux build outputs DS binary
./build.sh

#Windows build outputs DS_WIN.exe
TARGET=win ./build.sh
```

Game files and saves in `DSAVANT` are preserved across builds. Launch the
Linux build from the repository root with:

```sh
./run.sh
```
 
## New Controls

- **Alt+Enter:** toggle fullscreen.
- **F12:** save a screenshot.

## New Battle auto mode

Turn on **BATTLE AUTO MODE** in Game Configuration to enable **AUTO** battle
orders: Fight the nearest group when possible, otherwise Guard. It also
speeds through the round.
