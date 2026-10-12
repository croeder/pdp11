/* pdp11_rss_ops.c - translation of pdp11_rss_ops.py
 * Double-Operand RSS instructions - 07 0R SS through 07 7R SS
 *
 * Where a register pair is used (written below as "(Reg, Reg+1)"),
 * the first register contains the low-order portion of the operand
 * and must be an even numbered register.  The next higher numbered register
 * contains the high-order portion of the operand (or the remainder).
 * An exception is the multiply instruction; Reg may be odd,
 * but if it is, the high 16 bits of the result are not stored.
 *
 * TRANSLATION NOTES
 * - do_rss_op fetches the source with addressing mode 'B' (byte) as the
 *   Python does.
 * - DIV checks the numerator, not the denominator, for zero; a zero
 *   denominator then raises ZeroDivisionError in Python, which this
 *   translation reproduces by stopping the program.
 * - ASH's register_sign uses the register number (rDest & 0o100000), which
 *   is always 0.  ASHC shifts right by the full source value.  SOB
 *   multiplies.  All reproduced as written. */
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

InstrResult MUL(rss_ops *self, int rDest, pyint source)
{
    /* 07 0R SS MUL 4-31   (R, R+1) < (R, R+1) * (src)
     * The contents of the destination register and source taken as two's
     * complement integers are multiplied and stored in the destination
     * register and the succeeding register (if R is even).
     * If R is odd only the low order product is stored. */
    pyint dest = Registers_get(self->reg, rDest);

    pyint py_source = pythonifyPDP11Word(source);
    pyint py_dest = pythonifyPDP11Word(dest);
    pyint result = pythonifyPDP11Word(py_source) * pythonifyPDP11Word(py_dest);

    pyint n = 0;
    if (result < 0)
        n = 1;
    PSW_set_psw(self->psw, .n = n);
    PSW_set_z(self->psw, 0, result);
    PSW_set_psw(self->psw, .v = 0);
    PSW_set_psw(self->psw, .c = 0);  /* **** this needs to be handled */

    /* if rDest is even, store the low value in the +1 register */
    if (py_mod(rDest, 2) == 0) {
        pyint pdp11result = PDP11LongifyPythonInteger(result);
        pyint high_result = py_shr(pdp11result, 16);
        pyint low_result = pdp11result & MASK_WORD;
        Registers_set(self->reg, rDest + 1, low_result);
        return ok(high_result, "");   /* this is where we set the other register */
    }
    return ok(PDP11WordifyPythonInteger(result), "");
}

InstrResult DIV(rss_ops *self, int rDest, pyint denominator)
{
    /* 07 1R SS DIV 4-32   (R, R+1) < (R, R+1) / (src)
     * The 32-bit two's complement integer in R and Rvl is divided by the
     * source operand.  The quotient is left in R; the remainder in Rvl.
     * Division will be performed so that the remainder is of the same sign
     * as the dividend.  R must be even. */
    if (py_mod(rDest, 2) == 1)
        return ok(0, "DIV R was not even");

    pyint R = Registers_get(self->reg, rDest);          /* high-order word */
    pyint Rvl = Registers_get(self->reg, rDest + 1);    /* low-order word */
    pyint pynumerator = pythonifyPDP11Long(py_shl(R, 16) | Rvl);
    pyint pydenominator = pythonifyPDP11Word(denominator);

    if (pynumerator == 0) {
        /* *** divide by zero needs to trap */
        PSW_set_psw(self->psw, .v = 0, .c = 0);
        return ok(0, "DIV Divide by Zero");
    }

    pyint pyquotient = py_floordiv(pynumerator, pydenominator);
    pyint pyremainder = py_mod(pynumerator, pydenominator);

    /* Fix for error in the way Python does remainder */
    if (pynumerator < 0 || pydenominator < 0) {
        pyquotient = pyquotient + 1;
        pyremainder = pynumerator - (pydenominator * pyquotient);
    }

    pyint pdp11quotient = PDP11WordifyPythonInteger(pyquotient);

    Registers_set(self->reg, rDest, pdp11quotient);
    Registers_set(self->reg, rDest + 1, PDP11WordifyPythonInteger(pyremainder));
    PSW_set_n(self->psw, 0, pdp11quotient);
    PSW_set_z(self->psw, 0, pdp11quotient);
    PSW_set_psw(self->psw, .v = 0, .c = 0);
    return ok(pdp11quotient, "");
}

InstrResult ASH(rss_ops *self, int rDest, pyint source)
{
    /* 07 2R SS ASH arithmetic shift 4-33   R < R >> NN
     * The shift count is taken as the low order 6 bits of the source operand.
     * This number ranges from -32 to + 31.
     * Negative is a right shift and positive is a left shift. */
    pyint register_sign = rDest & 0100000;
    pyint shifts = source & 0037;
    pyint shift_sign = source & 0040;
    pyint result;
    if (shift_sign == 0)
        result = py_shl(Registers_get(self->reg, rDest), shifts);
    else
        result = py_shr(Registers_get(self->reg, rDest), shifts + 1);
    PSW_set_n(self->psw, 0, result);
    PSW_set_z(self->psw, 0, result);
    pyint result_sign = result & 0100000;
    pyint v = (result_sign != register_sign) ? 1 : 0;
    /* *** need to calculate Carry status */
    PSW_set_psw(self->psw, .v = v, .c = 0);
    return ok(result, "");
}

InstrResult ASHC(rss_ops *self, int rDest, pyint source)
{
    /* 07 3R SS ASHC arithmetic shift combined 4-34   (R, R+1) < (R, R+1) >> N */
    pyint result = py_shr(Registers_get(self->reg, rDest), source);
    PSW_set_n(self->psw, 0, result);
    PSW_set_z(self->psw, 0, result);
    return ok(result, "");
}

InstrResult XOR(rss_ops *self, int rDest, pyint source)
{
    /* 07 4R DD XOR 4-35   (dst) < R ^ (dst) */
    pyint result = Registers_get(self->reg, rDest) ^ source;
    PSW_set_n(self->psw, 0, result);
    PSW_set_z(self->psw, 0, result);
    return ok(result, "");
}

InstrResult SOB(rss_ops *self, int rDest, pyint source)
{
    /* 07 7R NN SOB subtract one and branch 4-63   R < R -1, then maybe branch */
    pyint result = Registers_get(self->reg, rDest) * source;
    return ok(result, "SOB unimplemented");
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
     * 15-9 opcode, 8-6 reg, 5-0 src */
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
    snprintf(r.assembly, sizeof r.assembly, "%s", name);
    AMGet d = AddressModes_addressing_mode_get(self->am, 'B', source);
    InstrResult ir = fn(self, register_, d.operand);
    char a[PYSTR];
    logging_info("    result:%s", py_oct(a, ir.result));
    Registers_set(self->reg, register_, ir.result);
    snprintf(r.report, sizeof r.report, "%s", ir.report);
    StopWatches_stop(self->sw, "rss");
    return r;
}
