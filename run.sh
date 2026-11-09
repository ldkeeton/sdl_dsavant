#!/usr/bin/env bash
# Build if needed, then run the game from its output directory.
#
#   ./run.sh [argument ...]          arguments go to the game
#   TARGET=win ./run.sh              run the Windows build under Wine
set -u
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
target=${TARGET:-linux}
"$here/build.sh" || exit 1
cd "$here/build_out/$target" || exit 1
if [[ $target == win ]]; then
    exec wine ./DS.exe "$@"
fi
exec ./DS "$@"
