/* pdp11_ss_ops.h - translation of pdp11_ss_ops.py (single-operand instructions) */
#ifndef PDP11_SS_OPS_H
#define PDP11_SS_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct ss_ops ss_ops;
/* B is 'B' for byte instructions, 0 ("") for word instructions */
typedef InstrResult (*ss_instruction)(ss_ops *self, pyint operand, char B);

struct ss_ops {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    AddressModes *am;
    StopWatches *sw;
};

/* single_operand_instructions / _names / _texts dictionaries:
 * returns 1 and fills the outputs if opcode is a key, else 0 (KeyError) */
int ss_ops_lookup(pyint opcode, ss_instruction *fn, const char **name, const char **text);

void ss_ops_init(ss_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw);
pyint ss_ops_byte_mask(ss_ops *self, char BW, pyint value, pyint target);
pyint ss_ops_mask(ss_ops *self, pyint value, char B);

InstrResult JMP(ss_ops *self, pyint operand, char B);
InstrResult SWAB(ss_ops *self, pyint operand, char B);
InstrResult CLR(ss_ops *self, pyint operand, char B);
InstrResult COM(ss_ops *self, pyint operand, char B);
InstrResult INC(ss_ops *self, pyint operand, char B);
InstrResult DEC(ss_ops *self, pyint operand, char B);
InstrResult NEG(ss_ops *self, pyint operand, char B);
InstrResult ADC(ss_ops *self, pyint operand, char B);
InstrResult SBC(ss_ops *self, pyint operand, char B);
InstrResult TST(ss_ops *self, pyint operand, char B);
InstrResult ROR(ss_ops *self, pyint operand, char B);
InstrResult ROL(ss_ops *self, pyint operand, char B);
InstrResult ASR(ss_ops *self, pyint operand, char B);
InstrResult ASL(ss_ops *self, pyint operand, char B);
InstrResult SXT(ss_ops *self, pyint operand, char B);
InstrResult ss_MARK(ss_ops *self, pyint operand, char B);
InstrResult MTPS(ss_ops *self, pyint operand, char B);
InstrResult ss_MFPI(ss_ops *self, pyint operand, char B);
InstrResult ss_MTPI(ss_ops *self, pyint operand, char B);
InstrResult MFPS(ss_ops *self, pyint operand, char B);
InstrResult MFPD(ss_ops *self, pyint operand, char B);
InstrResult MTPD(ss_ops *self, pyint operand, char B);

int ss_ops_is_ss_op(ss_ops *self, pyint instruction);
OpResult ss_ops_do_ss_op(ss_ops *self, pyint instruction);

#endif
