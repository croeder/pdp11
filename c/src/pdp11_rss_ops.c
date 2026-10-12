/* pdp11_rss_ops.c - translation of pdp11_rss_ops.py
 * Double-Operand RSS instructions - 07 0R SS through 07 7R SS (EIS, XOR, SOB)
 *
 * The instruction semantics follow the DEC processor handbook (and SIMH for
 * the DIV error flags) rather than the Python; each FIX comment says what
 * the Python did.  The translation that reproduces the Python exactly is
 * tagged c-translation-faithful. */
#include "pdp11_rss_ops.h"
#include "pdp11_logger.h"

#include <stdio.h>
#include <string.h>

static InstrResult ok(pyint result, const char *report)
{
    InstrResult r;
    r.result = result;
    snprintf(r.report, sizeof r.report, "%s", report);
    return r;
}

void rss_ops_init(rss_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw)
{
    logging_info("initializing doubleOperandOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->am = am;
    self->sw = sw;
}

static const struct {
    pyint opcode;
    rss_instruction fn;
    const char *name;
} double_operand_RSS_instructions[] = {
    {0070000, MUL, "MUL"},
    {0071000, DIV, "DIV"},
    {0072000, ASH, "ASH"},
    {0073000, ASHC, "ASHC"},
    {0074000, XOR, "XOR"},
    {0077000, SOB, "SOB"},
};

int rss_ops_lookup(pyint opcode, rss_instruction *fn, const char **name)
{
    for (size_t i = 0; i < sizeof double_operand_RSS_instructions / sizeof double_operand_RSS_instructions[0]; i++) {
        if (double_operand_RSS_instructions[i].opcode == opcode) {
            if (fn) *fn = double_operand_RSS_instructions[i].fn;
            if (name) *name = double_operand_RSS_instructions[i].name;
            return 1;
        }
    }
    return 0;
}

/* sign-extend a 16- or 32-bit PDP-11 value */
static pyint sx16(pyint v) { v &= MASK_WORD; return (v & MASK_WORD_MSB) ? v - 0200000 : v; }
static pyint sx32(pyint v) { v &= MASK_LONG; return (v & MASK_LONG_MSB) ? v - 040000000000LL : v; }

InstrResult MUL(rss_ops *self, int rDest, pyint source)
{
    /* 07 0R SS MUL 4-31   (R, R|1) < R * (src)
     * High word to R, low word to R|1 (for odd R, R receives the low word).
     * N, Z on the 32-bit product; V cleared; C set if it does not fit in 16 bits.
     * FIX: the Python never set C. */
    pyint p = sx16(Registers_get(self->reg, rDest)) * sx16(source);
    Registers_set(self->reg, rDest, py_shr(p, 16));
    Registers_set(self->reg, rDest | 1, p);
    PSW_set_psw(self->psw, .n = p < 0, .z = p == 0, .v = 0, .c = (p > 077777 || p < -0100000));
    return ok(0, "");
}

InstrResult DIV(rss_ops *self, int rDest, pyint denominator)
{
    /* 07 1R SS DIV 4-32   (R, R|1) < (R, R|1) / (src)
     * Quotient to R, remainder (same sign as the dividend) to R|1.
     * Divide by zero: V and C set, registers unchanged.  Quotient overflow:
     * V set, registers unchanged.  (Flags in the error cases follow SIMH.)
     * FIX: the Python tested the numerator for zero, raised on a zero
     * denominator, and its quotient correction was wrong for exact negative
     * divisions and for two negative operands. */
    pyint num = py_shl(Registers_get(self->reg, rDest), 16) | Registers_get(self->reg, rDest | 1);
    pyint den = denominator & MASK_WORD;
    if (den == 0) {
        PSW_set_psw(self->psw, .n = 0, .z = 1, .v = 1, .c = 1);
        return ok(0, "DIV divide by zero");
    }
    pyint n = sx32(num), d = sx16(den);
    pyint q = n / d;   /* C division truncates toward zero, as the PDP-11 does */
    if (q > 077777 || q < -0100000) {
        PSW_set_psw(self->psw, .n = q < 0, .z = 0, .v = 1, .c = 0);
        return ok(0, "DIV overflow");
    }
    Registers_set(self->reg, rDest, q);
    Registers_set(self->reg, rDest | 1, n - d * q);
    PSW_set_psw(self->psw, .n = q < 0, .z = q == 0, .v = 0, .c = 0);
    return ok(0, "");
}

/* Shift a value of `bits` bits by count (low 6 bits of src, -32..31).
 * Sets *v if the sign changed during a left shift and *c to the last bit
 * shifted out. */
static pyint arith_shift(pyint value, int bits, pyint count, pyint *v, pyint *c)
{
    pyint mask = py_shl(1, bits) - 1, sign = py_shl(1, bits - 1);
    pyint n = count & 077;
    *v = 0;
    *c = 0;
    value &= mask;
    if (n & 040) {                 /* right shift by 64 - n */
        for (pyint i = 0; i < 0100 - n; i++) {
            *c = value & 1;
            value = py_shr(value, 1) | (value & sign);
        }
    } else {                       /* left shift by n */
        for (pyint i = 0; i < n; i++) {
            *c = (value & sign) ? 1 : 0;
            pyint shifted = py_shl(value, 1) & mask;
            if ((shifted & sign) != (value & sign)) *v = 1;
            value = shifted;
        }
    }
    return value;
}

InstrResult ASH(rss_ops *self, int rDest, pyint source)
{
    /* 07 2R SS ASH arithmetic shift 4-33
     * Shift count is the low 6 bits of the source, -32..+31; negative is right.
     * V: sign changed during the shift;  C: last bit shifted out.
     * FIX: the Python took the sign from the register number and never set C. */
    pyint v, c;
    pyint r = arith_shift(Registers_get(self->reg, rDest), 16, source, &v, &c);
    Registers_set(self->reg, rDest, r);
    PSW_set_psw(self->psw, .n = (r & MASK_WORD_MSB) ? 1 : 0, .z = r == 0, .v = v, .c = c);
    return ok(0, "");
}

InstrResult ASHC(rss_ops *self, int rDest, pyint source)
{
    /* 07 3R SS ASHC arithmetic shift combined 4-34
     * (R, R|1) treated as one 32-bit value, high word in R.  For odd R both
     * halves are R, so a right shift rotates it.
     * FIX: the Python shifted R right by the whole source value. */
    pyint v, c;
    pyint value = py_shl(Registers_get(self->reg, rDest), 16) | Registers_get(self->reg, rDest | 1);
    pyint r = arith_shift(value, 32, source, &v, &c);
    Registers_set(self->reg, rDest, py_shr(r, 16));
    Registers_set(self->reg, rDest | 1, r);
    PSW_set_psw(self->psw, .n = (r & MASK_LONG_MSB) ? 1 : 0, .z = r == 0, .v = v, .c = c);
    return ok(0, "");
}

InstrResult XOR(rss_ops *self, int rDest, pyint dest)
{
    /* 07 4R DD XOR 4-35   (dst) < R ^ (dst); R unaffected.
     * N, Z; V cleared; C unaffected.  do_rss_op writes the result to dst.
     * FIX: the Python wrote the result into R and did not clear V. */
    pyint r = (Registers_get(self->reg, rDest) ^ dest) & MASK_WORD;
    PSW_set_psw(self->psw, .n = (r & MASK_WORD_MSB) ? 1 : 0, .z = r == 0, .v = 0);
    return ok(r, "");
}

InstrResult SOB(rss_ops *self, int rDest, pyint offset)
{
    /* 07 7R NN SOB subtract one and branch 4-63
     * R < R - 1; if R != 0, PC < PC - 2 * NN.  Condition codes unaffected.
     * FIX: the Python multiplied R by the operand. */
    pyint r = (Registers_get(self->reg, rDest) - 1) & MASK_WORD;
    Registers_set(self->reg, rDest, r);
    if (r != 0)
        Registers_set_pc(self->reg, Registers_get_pc(self->reg) - 2 * (offset & 077), "SOB");
    return ok(0, "");
}

int rss_ops_is_rss_op(rss_ops *self, pyint instruction)
{
    /* bit 15 = 0;  bits 14-12 = 7;  bits 9 10 11 in [0,1,2,3,4,7] */
    (void)self;
    int bit15 = (instruction & 0100000) == 0000000;
    int bits14_12 = (instruction & 0070000) == 0070000;
    pyint b = instruction & 0077000;
    int bits11_9 = b == 0070000 || b == 0071000 || b == 0072000 || b == 0073000 || b == 0074000 || b == 0077000;
    return bit15 && bits14_12 && bits11_9;
}

OpResult rss_ops_do_rss_op(rss_ops *self, pyint instruction)
{
    /* dispatch an RSS opcode of form 0 111 *** *** *** ***
     * 15-9 opcode, 8-6 reg, 5-0 src (dst for XOR, offset for SOB)
     * Each instruction now stores its own results.
     * FIX: the Python fetched the source as a byte and stored every result in R. */
    StopWatches_start(self->sw, "rss");
    OpResult r = {0};
    pyint opcode = instruction & 0077000;
    int register_ = (int)py_shr(instruction & 0000700, 6);
    pyint source = instruction & 0000077;

    r.run = 1;
    rss_instruction fn;
    const char *name;
    if (!rss_ops_lookup(opcode, &fn, &name))
        PY_RAISE("KeyError", "double_operand_RSS_instruction_names");

    InstrResult ir;
    if (opcode == 0077000) {             /* SOB R,NN */
        char o[PYSTR];
        snprintf(r.assembly, sizeof r.assembly, "%s R%d,%s", name, register_, py_oct(o, source));
        ir = fn(self, register_, source);
    } else if (opcode == 0074000) {      /* XOR R,DD */
        /* R is read before the destination is evaluated, so XOR R1,(R1)+
         * uses R1's value from before the increment (as on the 11/03). */
        pyint rvalue = Registers_get(self->reg, register_);
        AMGet d = AddressModes_addressing_mode_get(self->am, 'W', source);
        snprintf(r.assembly, sizeof r.assembly, "%s R%d,%s", name, register_, d.assembly);
        snprintf(r.operand1, sizeof r.operand1, "%s", d.operand_word);
        pyint result = (rvalue ^ d.operand) & MASK_WORD;
        PSW_set_psw(self->psw, .n = (result & MASK_WORD_MSB) ? 1 : 0, .z = result == 0, .v = 0);
        AddressModes_addressing_mode_set(self->am, 'W', d.addressmode, result, d.register_, d.address);
        ir = (InstrResult){result, ""};
    } else {
        AMGet d = AddressModes_addressing_mode_get(self->am, 'W', source);
        /* MUL/DIV/ASH/ASHC SS,R */
        snprintf(r.assembly, sizeof r.assembly, "%s %s,R%d", name, d.assembly, register_);
        snprintf(r.operand1, sizeof r.operand1, "%s", d.operand_word);
        ir = fn(self, register_, d.operand);
    }
    snprintf(r.report, sizeof r.report, "%s", ir.report);
    StopWatches_stop(self->sw, "rss");
    return r;
}
