/* pdp11_cc_ops.c - translation of pdp11_cc_ops.py
 * See pdp11-40 page 4-79.  The cvzn condition codes are mapped into bits 3-0
 * of these operations; set and clear are distinguished by bit 4. */
#include "pdp11_cc_ops.h"
#include "pdp11_logger.h"

#include <stdio.h>

void cc_ops_init(cc_ops *self, PSW *psw, StopWatches *sw)
{
    logging_info("initializing ConditionCodeOps");
    self->psw = psw;
    self->sw = sw;
    self->set_opcode = 0000260;
    self->clear_opcode = 0000240;
    self->psw_bits = 0000017;
}

int cc_ops_is_cc_op(cc_ops *self, pyint instruction)
{
    /* returns true if the instruction is a condition-code instruction */
    (void)self;
    return (0000240 <= instruction) & (instruction <= 0000277);
}

OpResult cc_ops_do_cc_op(cc_ops *self, pyint instruction)
{
    /* dispatch a condition code instruction */
    StopWatches_start(self->sw, "cc");
    OpResult r = {0};
    char o[PYSTR];
    if ((instruction & self->set_opcode) == self->set_opcode) {
        /* As in the Python, "set" replaces all four bits rather than OR-ing them in. */
        pyint set_bits = instruction & self->psw_bits;
        snprintf(r.assembly, sizeof r.assembly, "SETCC %s", py_oct(o, set_bits));
        PSW_set_nzvc(self->psw, set_bits);
    } else if ((instruction & self->clear_opcode) == self->clear_opcode) {
        pyint clr_bits = instruction & self->psw_bits;
        pyint set_bits = self->psw->psw & ~clr_bits;
        snprintf(r.assembly, sizeof r.assembly, "CLRCC %s", py_oct(o, clr_bits));
        PSW_set_nzvc(self->psw, set_bits);
    } else {
        /* Python: assembly is unbound here (UnboundLocalError) */
        PY_RAISE("UnboundLocalError", "assembly referenced before assignment");
    }
    StopWatches_stop(self->sw, "cc");
    r.run = 1;
    return r;
}
