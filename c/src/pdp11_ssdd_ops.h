/* pdp11_ssdd_ops.h - translation of pdp11_ssdd_ops.py (double-operand SSDD instructions) */
#ifndef PDP11_SSDD_OPS_H
#define PDP11_SSDD_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct ssdd_ops ssdd_ops;
/* BW is 'B' for byte instructions, 'W' for word instructions */
typedef InstrResult (*ssdd_instruction)(ssdd_ops *self, char BW, pyint source, pyint dest);

struct ssdd_ops {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    AddressModes *am;
    StopWatches *sw;
};

/* double_operand_SSDD_instructions, keyed by instruction & 0o070000 */
ssdd_instruction ssdd_ops_instruction(pyint opcode);
/* double_operand_SSDD_instruction_names, keyed by instruction & 0o170000 */
const char *ssdd_ops_name(pyint name_opcode);

void ssdd_ops_init(ssdd_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw);
pyint ssdd_ops_sign(ssdd_ops *self, pyint a);
pyint ssdd_ops_byte_mask(ssdd_ops *self, char BW, pyint value, pyint target);

InstrResult MOV(ssdd_ops *self, char BW, pyint source, pyint dest);
InstrResult CMP(ssdd_ops *self, char BW, pyint source, pyint dest);
InstrResult BIT(ssdd_ops *self, char BW, pyint source, pyint dest);
InstrResult BIC(ssdd_ops *self, char BW, pyint source, pyint dest);
InstrResult BIS(ssdd_ops *self, char BW, pyint source, pyint dest);
InstrResult ADDSUB(ssdd_ops *self, char BW, pyint source, pyint dest);

int ssdd_ops_is_ssdd_op(ssdd_ops *self, pyint instruction);
OpResult ssdd_ops_do_ssdd_op(ssdd_ops *self, pyint instruction);

#endif
