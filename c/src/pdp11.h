/* pdp11.h - translation of pdp11.py: startup, instruction dispatch, processor loop */
#ifndef PDP11_H
#define PDP11_H

#include "pdp11_boot.h"
#include "pdp11_br_ops.h"
#include "pdp11_cc_ops.h"
#include "pdp11_config.h"
#include "pdp11_dl11.h"
#include "pdp11_hardware.h"
#include "pdp11_m9301.h"
#include "pdp11_noopr_ops.h"
#include "pdp11_other_ops.h"
#include "pdp11_rk11.h"
#include "pdp11_rss_ops.h"
#include "pdp11_ss_ops.h"
#include "pdp11_ssdd_ops.h"
#include "pdp11_terminal.h"
#include "stopwatches.h"

typedef struct {
    StopWatches sw;
    Lock lock;

    /* hardware */
    Registers reg;
    Ram ram;
    PSW psw;
    Stack stack;
    AddressModes am;

    /* operations */
    br_ops br;
    noopr_ops noopr_ops;
    ss_ops ss_ops;
    ssdd_ops ssdd_ops;
    rss_ops rss_ops;
    other_ops other_ops;
    cc_ops cc_ops;

    /* executed: dictionary of instruction -> "instruction,assembly",
     * reported in first-seen order like a Python dict */
    char **executed;
    int *executed_order;
    int executed_count;
    pyint CPU_cycles;

    int run;

    /* i/o devices */
    pdp11Boot boot;
    M9301 m9301;
    DL11 dl11;
    Terminal terminal;
} PDP11;

/* Python: PDP11(ui="None").  Only the stdio terminal exists in C, so ui is
 * logged and otherwise ignored. */
void PDP11_init(PDP11 *self, const char *ui);
void PDP11_free(PDP11 *self);
void PDP11_set_run(PDP11 *self, int new_run);
int  PDP11_get_run(PDP11 *self);
OpResult PDP11_dispatch_opcode(PDP11 *self, pyint instruction);
int  PDP11_instruction_cycle(PDP11 *self);

typedef struct {
    PDP11 *pdp11;
} pdp11Run;

void pdp11Run_init(pdp11Run *self, PDP11 *pdp11);
/* Run PDP11 emulator with the stdio terminal.  limit < 0 means no limit. */
void pdp11Run_run(pdp11Run *self, pyint limit);

#endif
