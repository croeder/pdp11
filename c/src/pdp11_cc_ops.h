/* pdp11_cc_ops.h - translation of pdp11_cc_ops.py (condition code operators) */
#ifndef PDP11_CC_OPS_H
#define PDP11_CC_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct {
    PSW *psw;
    StopWatches *sw;
    pyint set_opcode;
    pyint clear_opcode;
    pyint psw_bits;
} cc_ops;

void cc_ops_init(cc_ops *self, PSW *psw, StopWatches *sw);
int cc_ops_is_cc_op(cc_ops *self, pyint instruction);
OpResult cc_ops_do_cc_op(cc_ops *self, pyint instruction);

#endif
