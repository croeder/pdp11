/* pdp11_noopr_ops.h - translation of pdp11_noopr_ops.py
 * no-operand instructions 00 00 00 through 00 00 06 */
#ifndef PDP11_NOOPR_OPS_H
#define PDP11_NOOPR_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    Stack *stack;
    StopWatches *sw;
} noopr_ops;

/* no_operand_instruction_names: returns the name, or NULL (KeyError) */
const char *noopr_ops_name(pyint instruction);

void noopr_ops_init(noopr_ops *self, Registers *reg, Ram *ram, PSW *psw, Stack *stack, StopWatches *sw);
int noopr_ops_is_noopr_op(noopr_ops *self, pyint instruction);
OpResult noopr_ops_do_noopr_op(noopr_ops *self, pyint instruction);

#endif
