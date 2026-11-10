#!/usr/bin/env bash
# Launch the Linux build with the game directory as the working directory.
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
game_dir="$repo_dir/DSAVANT"

if [[ ! -x "$game_dir/DS" ]]; then
    printf 'Game binary not found: %s\nRun ./build.sh first.\n' "$game_dir/DS" >&2
    exit 1
fi

cd -- "$game_dir"
exec ./DS "$@"
