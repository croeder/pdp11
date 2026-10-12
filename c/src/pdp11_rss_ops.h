/* pdp11_rss_ops.h - translation of pdp11_rss_ops.py
 * Double-Operand RSS instructions - 07 0R SS through 07 7R SS */
#ifndef PDP11_RSS_OPS_H
#define PDP11_RSS_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct rss_ops rss_ops;
typedef InstrResult (*rss_instruction)(rss_ops *self, int rDest, pyint source);

struct rss_ops {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    AddressModes *am;
    StopWatches *sw;
};

/* double_operand_RSS_instructions / _names, keyed by instruction & 0o077000 */
int rss_ops_lookup(pyint opcode, rss_instruction *fn, const char **name);

void rss_ops_init(rss_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw);

InstrResult MUL(rss_ops *self, int rDest, pyint source);
InstrResult DIV(rss_ops *self, int rDest, pyint denominator);
InstrResult ASH(rss_ops *self, int rDest, pyint source);
InstrResult ASHC(rss_ops *self, int rDest, pyint source);
InstrResult XOR(rss_ops *self, int rDest, pyint source);
InstrResult SOB(rss_ops *self, int rDest, pyint source);

int rss_ops_is_rss_op(rss_ops *self, pyint instruction);
OpResult rss_ops_do_rss_op(rss_ops *self, pyint instruction);

#endif
