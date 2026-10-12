/* pdp11_terminal.h - translation of pdp11_terminal.py, as a stdin/stdout terminal.
 *
 * The Python Terminal class was a placeholder (its cycle() refers to an
 * undefined name and it has no keyboard input), and the working consoles
 * were PySimpleGUI windows.  This C version connects the DL11 to the
 * process's own terminal: characters the PDP-11 transmits are written to
 * `out`, and keys typed on stdin are delivered to the DL11 receiver. */
#ifndef PDP11_TERMINAL_H
#define PDP11_TERMINAL_H

#include <stdio.h>

#include "pdp11_dl11.h"
#include "stopwatches.h"

typedef struct {
    DL11 *dl11;
    pyint cycles_since_window;
    pyint buffer;    /* for holding LF after CR was sent */
    StopWatches *sw;
    FILE *out;       /* where transmitted characters go (stdout by default) */
    int input_fd;    /* where keys come from (stdin by default); -1 for none */
} Terminal;

void Terminal_init(Terminal *self, DL11 *dl11, StopWatches *sw);
/* One cycle for the terminal: deliver output, poll for input. */
void Terminal_cycle(Terminal *self);
/* Put stdin into character-at-a-time, no-echo mode (restored at exit). */
void Terminal_raw_input(Terminal *self);

#endif
