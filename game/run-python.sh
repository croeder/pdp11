#!/bin/sh
# Runs the game on the Python emulator (slowly); see run.sh.
exec "$(dirname "$0")/run.sh" python "$@"
