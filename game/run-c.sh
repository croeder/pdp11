#!/bin/sh
# Runs the game on the C emulator; see run.sh.
exec "$(dirname "$0")/run.sh" c "$@"
