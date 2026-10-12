/* pdp11_br_ops.h - translation of pdp11_br_ops.py (branch instructions) */
#ifndef PDP11_BR_OPS_H
#define PDP11_BR_OPS_H

#include "pdp11_hardware.h"
#include "pdp11_op_result.h"
#include "stopwatches.h"

typedef struct br_ops br_ops;
typedef int (*br_instruction)(br_ops *self, pyint offset);

struct br_ops {
    Registers *reg;
    Ram *ram;
    PSW *psw;
    StopWatches *sw;
};

void br_ops_init(br_ops *self, Registers *reg, Ram *ram, PSW *psw, StopWatches *sw);

/* branch_instructions / branch_instruction_names dictionaries:
 * returns 1 and fills fn/name if opcode is a key, else 0 (KeyError) */
int br_ops_lookup(pyint opcode, br_instruction *fn, const char **name);

int BR(br_ops *self, pyint offset);
int BNE(br_ops *self, pyint offset);
int BEQ(br_ops *self, pyint offset);
int BGE(br_ops *self, pyint offset);
int BLT(br_ops *self, pyint offset);
int BGT(br_ops *self, pyint offset);
int BLE(br_ops *self, pyint offset);
int BPL(br_ops *self, pyint offset);
int BMI(br_ops *self, pyint offset);
int BHI(br_ops *self, pyint offset);
int BLOS(br_ops *self, pyint offset);
int BVC(br_ops *self, pyint offset);
int BVS(br_ops *self, pyint offset);
int BCC(br_ops *self, pyint offset);
int BCS(br_ops *self, pyint offset);

int br_ops_is_br_op(br_ops *self, pyint instruction);
OpResult br_ops_do_br_op(br_ops *self, pyint instruction);

#endif
