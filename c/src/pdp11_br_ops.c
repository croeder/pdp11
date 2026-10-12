/* pdp11_br_ops.c - translation of pdp11_br_ops.py (branch instructions)
 *
 * MASK_HIGH_BYTE - opcode
 * MASK_LOW_BYTE - offset
 * extend sign of offset through bits 8-15, multiply by two,
 * add to PC to make branch address
 *
 * TRANSLATION NOTE: Python's comparison operators bind more loosely than its
 * bitwise operators, so `a | b == 0` means `(a | b) == 0`.  The C below
 * spells that grouping out with parentheses.  BGE is fixed to the DEC
 * definition (see FIX); the other conditions already matched it. */
#include "pdp11_br_ops.h"
#include "pdp11_logger.h"

#include <stdio.h>

void br_ops_init(br_ops *self, Registers *reg, Ram *ram, PSW *psw, StopWatches *sw)
{
    logging_info("initializing branchOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->sw = sw;
}

static const struct {
    pyint opcode;
    br_instruction fn;
    const char *name;
} branch_instructions[] = {
    {0000400, BR, "BR"},
    {0001000, BNE, "BNE"},
    {0001400, BEQ, "BEQ"},
    {0002000, BGE, "BGE"},
    {0002400, BLT, "BLT"},
    {0003000, BGT, "BGT"},
    {0003400, BLE, "BLE"},
    {0100000, BPL, "BPL"},
    {0100400, BMI, "BMI"},
    {0101000, BHI, "BHI"},
    {0101400, BLOS, "BLOS"},
    {0102000, BVC, "BVC"},
    {0102400, BVS, "BVS"},
    {0103000, BCC, "BCC"},   /* BCC = BHIS */
    {0103400, BCS, "BCS"},   /* BCS = BLO */
};

int br_ops_lookup(pyint opcode, br_instruction *fn, const char **name)
{
    for (size_t i = 0; i < sizeof branch_instructions / sizeof branch_instructions[0]; i++) {
        if (branch_instructions[i].opcode == opcode) {
            if (fn) *fn = branch_instructions[i].fn;
            if (name) *name = branch_instructions[i].name;
            return 1;
        }
    }
    return 0;
}

/* Symbols in DEC book and Python operators
 * A = boolean AND = &,  V = boolean OR = |,  -V = exclusive OR = ^,  ~ = NOT */

int BR(br_ops *self, pyint offset)
{
    /* 00 04 XXX Branch */
    Registers_set_pc_2x_offset(self->reg, offset, "BR");
    return 1;
}

int BNE(br_ops *self, pyint offset)
{
    /* 00 10 XXX branch if not equal z=0 */
    if (PSW_get_z(self->psw) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BNE");
    return 1;
}

int BEQ(br_ops *self, pyint offset)
{
    /* 00 14 XXX branch if equal z=1 */
    if (PSW_get_z(self->psw) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BEQ");
    return 1;
}

int BGE(br_ops *self, pyint offset)
{
    /* 00 20 XXX branch if greater than or equal 4-47: N xor V = 0
     * FIX: the Python tested N or V. */
    if ((PSW_get_n(self->psw) ^ PSW_get_v(self->psw)) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BGE");
    return 1;
}

int BLT(br_ops *self, pyint offset)
{
    /* 00 24 XXX branch if less than zero */
    if ((PSW_get_n(self->psw) ^ PSW_get_v(self->psw)) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BLT");
    return 1;
}

int BGT(br_ops *self, pyint offset)
{
    /* 00 30 XXX */
    if ((PSW_get_z(self->psw) | (PSW_get_n(self->psw) ^ PSW_get_v(self->psw))) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BTG");
    return 1;
}

int BLE(br_ops *self, pyint offset)
{
    /* 00 34 XXX */
    if ((PSW_get_z(self->psw) | (PSW_get_n(self->psw) ^ PSW_get_v(self->psw))) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BLE");
    return 1;
}

int BPL(br_ops *self, pyint offset)
{
    /* 01 00 XXX branch if positive n=0 */
    if (PSW_get_n(self->psw) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BPL");
    return 1;
}

int BMI(br_ops *self, pyint offset)
{
    /* 10 04 XXX branch if negative n=1 */
    if (PSW_get_n(self->psw) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BMI");
    return 1;
}

int BHI(br_ops *self, pyint offset)
{
    /* 10 10 XXX branch if higher */
    if (PSW_get_c(self->psw) == 0 && PSW_get_z(self->psw) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BHI");
    return 1;
}

int BLOS(br_ops *self, pyint offset)
{
    /* 10 14 XXX branch if lower or same */
    if ((PSW_get_c(self->psw) | PSW_get_z(self->psw)) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BLOS");
    return 1;
}

int BVC(br_ops *self, pyint offset)
{
    /* 10 20 XXX Branch if overflow is clear v=0 */
    if (PSW_get_v(self->psw) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BVC");
    return 1;
}

int BVS(br_ops *self, pyint offset)
{
    /* 10 24 XXX Branch if overflow is set v=1 */
    if (PSW_get_v(self->psw) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BVS");
    return 1;
}

int BCC(br_ops *self, pyint offset)
{
    /* 10 30 XXX branch if Carry Clear, BHIS is the same as BCC */
    if (PSW_get_c(self->psw) == 0)
        Registers_set_pc_2x_offset(self->reg, offset, "BCC");
    return 1;
}

int BCS(br_ops *self, pyint offset)
{
    /* 10 34 XXX branch if lower. BCS is the same as BLO */
    if (PSW_get_c(self->psw) == 1)
        Registers_set_pc_2x_offset(self->reg, offset, "BCS");
    return 1;
}

int br_ops_is_br_op(br_ops *self, pyint instruction)
{
    /* *0 ** xxx
     * bit 15 can be 1 or 0; mask = 0o100000
     * bits 14-12 = 0; mask = 0o070000
     * bit 11,10,9,8; mask = 0o007400
     * bits 7,6, 5,4,3, 2,1,0 are the offset; mask = 0o000377 */
    (void)self;
    int blankbits = (instruction & 0070000) == 0000000;
    pyint masked = instruction & 0107400;
    int lowbits0 = masked == 0000400 || masked == 0001000 || masked == 0001400 || masked == 0002000 ||
                   masked == 0002400 || masked == 0003000 || masked == 0003400;
    int lowbits1 = masked == 0100000 || masked == 0100400 || masked == 0101000 || masked == 0101400 ||
                   masked == 0102000 || masked == 0102400 || masked == 0103000 || masked == 0103400;
    return blankbits && (lowbits0 || lowbits1);
}

OpResult br_ops_do_br_op(br_ops *self, pyint instruction)
{
    /* dispatch a branch opcode of form X 000 0XX X** *** *** */
    StopWatches_start(self->sw, "br");
    OpResult r = {0};
    pyint opcode = instruction & MASK_HIGH_BYTE;
    pyint offset = instruction & MASK_LOW_BYTE;
    br_instruction fn;
    const char *name;
    if (br_ops_lookup(opcode, &fn, &name)) {
        char o[PYSTR];
        snprintf(r.assembly, sizeof r.assembly, "%s %s", name, py_oct(o, offset));
        r.run = fn(self, offset);
    } else {
        snprintf(r.assembly, sizeof r.assembly, "Error: branch opcode not found");
        r.run = 0;
    }
    StopWatches_stop(self->sw, "br");
    return r;
}
