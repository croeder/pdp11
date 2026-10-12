/* runpdp11.c - translation of runpdp11.py: run PDP11 in a terminal.
 *
 * Powers on the PDP11 (which loads the M9301-YA boot ROM and starts at its
 * console/diagnostics entry point) and runs it on this terminal.
 * Usage: runpdp11 [instruction-limit]     (default: no limit; Ctrl-C stops)
 * Run it from the directory that holds config_default.json and source/. */
#include <stdlib.h>

#include "pdp11.h"

int main(int argc, char **argv)
{
    pyint limit = argc > 1 ? strtoll(argv[1], NULL, 10) : -1;

    /* instantiate the PDP11 */
    static PDP11 pdp11;
    PDP11_init(&pdp11, NULL);
    pdp11Run run;
    pdp11Run_init(&run, &pdp11);
    Terminal_raw_input(&pdp11.terminal);

    /* power on */
    pdp11Run_run(&run, limit);
    PDP11_free(&pdp11);
    return 0;
}
