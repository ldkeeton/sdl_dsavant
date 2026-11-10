#!/usr/bin/env bash
# Build a selected target with SDL3. Run from anywhere; paths are
# relative to this repository.
#
#   ./build.sh                  DSAVANT/DS (Linux)
#   TARGET=win ./build.sh       DSAVANT/DS_WIN.exe, SDL3.dll (Windows)
#   RELEASE=1 ./build.sh        stripped release executable
#
# Executables and the Windows SDL runtime go into DSAVANT, which is created
# if absent. Game files and saves there are preserved. Objects go to
# build_out/<platform>_obj/ and are recreated on each build.
#
# Environment
#   TARGET             linux (default) or win
#   JOBS               parallel jobs (default: number of CPUs)
#   FORCE=1            accepted for compatibility; builds always start clean
#   STUBS=0            no stubs for undefined symbols: the link fails instead
#   COMPILE_ONLY=1     compile, do not link
#   RELEASE=1         strip symbols from the linked executable
#   MINGW_SDL3_PREFIX  where the SDL3 mingw release is installed
#                      (default /usr/local/x86_64-w64-mingw32)
#
# How the build works is described at the top of tools/build.py.
set -u
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$here/tools/build.py" "$@"
