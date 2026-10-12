/* pdp11_other_ops.h - translation of pdp11_other_ops.py (other instructions) */
#ifndef PDP11_OTHER_OPS_H
#define PDP11_OTHER_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    AddressModes *am;
    Stack stack;   /* other_ops makes its own Stack(reg, ram, psw) */
    StopWatches *sw;
} other_ops;

void other_ops_init(other_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw);
int other_ops_is_other_op(other_ops *self, pyint instruction);
OpResult other_ops_do_other_op(other_ops *self, pyint instruction);

#endif
