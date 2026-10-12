/* runpdp11.c - translation of runpdp11.py: run PDP11 in a terminal.
 *
 * Powers on the PDP11 (which loads the M9301-YA boot ROM and starts at its
 * console/diagnostics entry point) and runs it on this terminal.
 * Usage: runpdp11 [-l listing] [instruction-limit]
 *   -l listing  also load a program ("address word" lines in octal, the
 *               format of source/M9301-YA.txt) and start at its first address
 *   instruction-limit  default: no limit; Ctrl-C stops
 * Run it from the directory that holds config_default.json and source/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdp11.h"

int main(int argc, char **argv)
{
    const char *listing = NULL;
    int arg = 1;
    if (arg + 1 < argc && !strcmp(argv[arg], "-l")) {
        listing = argv[arg + 1];
        arg += 2;
    }
    pyint limit = -1;
    char *end = NULL;
    if (arg < argc)
        limit = strtoll(argv[arg], &end, 10);
    if (argc > arg + 1 || (arg < argc && (*argv[arg] == '\0' || *end != '\0' || limit < 0))) {
        fprintf(stderr, "usage: runpdp11 [-l listing] [instruction-limit]\n");
        return 2;
    }

    /* instantiate the PDP11 */
    static PDP11 pdp11;
    PDP11_init(&pdp11, NULL);
    if (listing) {
        pyint base = pdp11Boot_read_pdp11_assembly_file(&pdp11.boot, listing);
        Registers_set_pc(&pdp11.reg, base, "runpdp11 -l");
    }
    pdp11Run run;
    pdp11Run_init(&run, &pdp11);
    Terminal_raw_input(&pdp11.terminal);

    /* power on */
    pdp11Run_run(&run, limit);
    PDP11_free(&pdp11);
    return 0;
}
