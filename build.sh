#!/usr/bin/env bash
# Build the game for the host with SDL3.  Run from anywhere; paths are
# relative to this repository.
#
#   ./build.sh                  Linux build: build_out/linux/DS
#   TARGET=win ./build.sh       Windows (mingw-w64): build_out/win/DS.exe
#
# Output goes to build_out/<target>/ with the original game's files copied
# from apps/DSAVANT (files already there, such as saves, are kept).  Objects
# and intermediate files are under build_out/<target>/obj/.
#
# Environment
#   TARGET             linux (default) or win
#   JOBS               parallel jobs (default: number of CPUs)
#   FORCE=1            rebuild everything
#   STUBS=0            no stubs for undefined symbols: the link fails instead
#   COMPILE_ONLY=1     compile, do not link
#   MINGW_SDL3_PREFIX  where the SDL3 mingw release is installed
#                      (default /usr/local/x86_64-w64-mingw32)
#
# How the build works is described at the top of tools/build.py.
set -u
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$here/tools/build.py" "$@"
