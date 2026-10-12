/* pdp11_ss_ops.c - translation of pdp11_ss_ops.py (single operand instructions)
 *
 * TRANSLATION NOTES
 * - Operands and results are Python ints (pyint): results are not always
 *   masked to 16 bits, and NEG can produce a negative number that is then
 *   stored in a register.  That is reproduced.
 * - The Python sets a high bit (| 0o400000) before shifting "for python
 *   weirdness"; kept, since it changes results in some cases.
 * - INC, DEC and NEG call psw.set_v(B, ...), which (see pdp11_hardware.c)
 *   writes the mode bits rather than V.  Reproduced.
 * - ROL byte uses 0o0000000010000000 for "bit 7" and ASL uses
 *   0o1000000000000000 for "bit 15"; both are octal literals, so they test
 *   bits 21 and 45.  Reproduced.
 * - ASL byte: the Python line
 *       result = (highbyte << 1) & MASK_HIGH_BYTE (lowbyte << 1) & MASK_LOW_BYTE
 *   is missing an operator, so ASLB raises TypeError in Python.  The C
 *   joins the two halves with |.
 * - SXT stores 1, not -1, when N is set.  Reproduced. */
#include "pdp11_ss_ops.h"
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

void ss_ops_init(ss_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw)
{
    logging_info("initializing SingleOperandOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->am = am;
    self->sw = sw;
}

/* ****************************************************
 * Single-Operand instructions -
 * 00 50 DD - 00 77 DD
 * 10 50 DD - 10 77 DD
 * **************************************************** */
static const struct {
    pyint opcode;
    ss_instruction fn;
    const char *name;
    const char *text;
} single_operand_instructions[] = {
    {0000100, JMP, "JMP", "jump"},
    {0000300, SWAB, "SWAB", "swap bytes"},

    {0005000, CLR, "CLR", "clear destination"},
    {0005100, COM, "COM", "complement"},
    {0005200, INC, "INC", "increment"},
    {0005300, DEC, "DEC", "decrement"},
    {0005400, NEG, "NEG", "negative"},
    {0005500, ADC, "ADC", "add carry"},
    {0005600, SBC, "SBC", "subtract carry"},
    {0005700, TST, "TST", "test"},
    {0006000, ROR, "ROR", "rotate right"},
    {0006100, ROL, "ROL", "rotate left"},
    {0006200, ASR, "ASR", "arithmetic shift right"},
    {0006300, ASL, "ASL", "arithmetic shift left"},
    {0006400, ss_MARK, "MARK", "subroutine cleanup"},
    {0006500, ss_MFPI, "MFPI", "move from previous instruction space"},
    {0006600, ss_MTPI, "MTPI", "move to previous instruction space"},
    {0006700, SXT, "SXT", "sign extend"},

    {0105000, CLR, "CLRB", "clear destination byte"},
    {0105100, COM, "COMB", "complement byte"},
    {0105200, INC, "INCB", "increment byte"},
    {0105300, DEC, "DECB", "decrement byte"},
    {0105400, NEG, "NEGB", "negative byte"},
    {0105500, ADC, "ADCB", "add carry byte"},
    {0105600, SBC, "SBCB", "subtract carry byte"},
    {0105700, TST, "TSTB", "test byte"},
    {0106000, ROR, "RORB", "rotate right byte"},
    {0106100, ROL, "ROLB", "rotate left byte"},
    {0106200, ASR, "ASRB", "arithmetic shift right byte"},
    {0106300, ASL, "ASLB", "arithmetic shift left byte"},
    {0106400, MTPS, "MTPS", "move to PSW"},
    {0106500, MFPD, "MFPD", "move from previous data space"},
    {0106600, MTPD, "MTPD", "move to previous data space"},
    {0106700, MFPS, "MFPS", "move from psw"},
};

int ss_ops_lookup(pyint opcode, ss_instruction *fn, const char **name, const char **text)
{
    for (size_t i = 0; i < sizeof single_operand_instructions / sizeof single_operand_instructions[0]; i++) {
        if (single_operand_instructions[i].opcode == opcode) {
            if (fn) *fn = single_operand_instructions[i].fn;
            if (name) *name = single_operand_instructions[i].name;
            if (text) *text = single_operand_instructions[i].text;
            return 1;
        }
    }
    return 0;
}

pyint ss_ops_byte_mask(ss_ops *self, char BW, pyint value, pyint target)
{
    /* mask source and result like PDP11 does.
     * For byte operations only the low byte is affected; the target's high byte remains. */
    (void)self;
    if (BW == 'B')
        return (value & MASK_LOW_BYTE) | (target & MASK_HIGH_BYTE);
    return value;
}

pyint ss_ops_mask(ss_ops *self, pyint value, char B)
{
    /* Apply the correct byte or word mask to the value */
    (void)self;
    return B == 'B' ? (value & MASK_LOW_BYTE) : (value & MASK_WORD);
}

InstrResult JMP(ss_ops *self, pyint operand, char B)
{
    /* 00 01 DD JMP jump 4-56 */
    (void)B;
    Registers_set_pc(self->reg, operand, "JMP");
    return ok(operand, "");
}

InstrResult SWAB(ss_ops *self, pyint operand, char B)
{
    /* 00 03 DD Swap Bytes 4-17
     * n: set if bit 7 of result is set;  z: set if low-order byte of result = 0
     * v: cleared;  c: cleared */
    (void)B;
    pyint result = py_shr(operand & 0xFF00, 8) + py_shl(operand & 0x00FF, 8);
    PSW_set_n(self->psw, 'B', result & 0x00FF);
    PSW_set_z(self->psw, 'B', result & 0x00FF);
    PSW_set_psw(self->psw, .v = 0, .c = 0);
    return ok(result, "");
}

InstrResult CLR(ss_ops *self, pyint operand, char B)
{
    /* 00 50 DD Clear Destination */
    pyint result = ss_ops_byte_mask(self, B, 0, operand);
    PSW_set_psw(self->psw, .n = 0, .z = 1, .v = 0, .c = 0);
    return ok(result, "");
}

InstrResult COM(ss_ops *self, pyint operand, char B)
{
    /* 00 51 DD Complement Destination
     * Because of the way Python does things, we add a leading bit and then strip it off. */
    pyint woperand = operand | 0400000;
    pyint result = ~woperand & MASK_WORD;
    result = ss_ops_byte_mask(self, B, result, operand);
    PSW_set_n(self->psw, B, result);
    PSW_set_z(self->psw, B, result);
    PSW_set_psw(self->psw, .v = 0, .c = 1);
    return ok(result, "");
}

InstrResult INC(ss_ops *self, pyint operand, char B)
{
    /* 00 52 DD Increment Destination */
    pyint result = operand + 1;
    result = ss_ops_byte_mask(self, B, result, operand);
    PSW_set_v(self->psw, B, result);
    PSW_set_n(self->psw, B, result);
    PSW_set_z(self->psw, B, result);
    return ok(result, "");
}

InstrResult DEC(ss_ops *self, pyint operand, char B)
{
    /* 00 53 DD Decrement Destination */
    pyint woperand = operand | 0400000;
    pyint result = (woperand - 1) & MASK_WORD;
    result = ss_ops_byte_mask(self, B, result, operand);
    PSW_set_v(self->psw, B, result);
    PSW_set_n(self->psw, B, result);
    PSW_set_z(self->psw, B, result);
    return ok(result, "");
}

InstrResult NEG(ss_ops *self, pyint operand, char B)
{
    /* 00 54 DD negate Destination */
    pyint result = -operand;
    result = ss_ops_byte_mask(self, B, result, operand);
    PSW_set_v(self->psw, B, result);
    PSW_set_n(self->psw, B, result);
    PSW_set_z(self->psw, B, result);
    return ok(result, "");
}

InstrResult ADC(ss_ops *self, pyint operand, char B)
{
    /* 00 55 DD Add Carry */
    pyint result = operand + PSW_get_c(self->psw);
    result = ss_ops_byte_mask(self, B, result, operand);
    return ok(result, "");
}

InstrResult SBC(ss_ops *self, pyint operand, char B)
{
    /* 00 56 DD Subtract Carry */
    pyint result = operand - PSW_get_c(self->psw);
    result = ss_ops_byte_mask(self, B, result, operand);
    return ok(result, "");
}

InstrResult TST(ss_ops *self, pyint operand, char B)
{
    /* 00 57 DD Test Destination */
    PSW_set_n(self->psw, B, operand);
    PSW_set_z(self->psw, B, operand);
    PSW_set_psw(self->psw, .v = 0, .c = 0);
    return ok(operand, "");
}

InstrResult ROR(ss_ops *self, pyint operand, char B)
{
    /* 00 60 DD ROR rotate right
     * Bit 0 is loaded into the C-bit and the previous carry into bit 15.
     * N: high-order bit of the result;  Z: result = 0
     * C: low-order bit of the destination;  V: N xor C */
    pyint result, carry, testresult;
    if (B == 'B') {
        pyint bit8 = operand & 0x0100;
        pyint highbyte = (operand & MASK_HIGH_BYTE) | 0400000;
        pyint bit0 = operand & 0x0001;
        pyint lowbyte = (operand & MASK_LOW_BYTE) | 0400000;
        result = py_shl(bit8, 7) | (py_shr(highbyte, 1) & MASK_HIGH_BYTE) | py_shl(bit0, 7) |
                 (py_shr(lowbyte, 1) & MASK_LOW_BYTE);
        carry = (result & 01) | py_shr(result & 0x0100, 8);
        testresult = result & MASK_LOW_BYTE;
    } else {
        operand = operand | 0400000;  /* set the high bit for python weirdness */
        pyint bit0 = operand & 0x0001;
        result = (py_shl(bit0, 15) | py_shr(operand, 1)) & MASK_WORD;
        carry = result & 01;
        testresult = result;
    }
    pyint N = PSW_set_n(self->psw, B, testresult);
    pyint Z = PSW_set_z(self->psw, B, testresult);
    pyint C = carry & 01;
    pyint V = N ^ C;
    PSW_set_psw(self->psw, .n = N, .z = Z, .v = V, .c = C);
    return ok(result, "");
}

InstrResult ROL(ss_ops *self, pyint operand, char B)
{
    /* 00 61 DD ROL rotate left
     * Bit 15 is loaded into the carry and the previous carry into bit 0.
     * V: N xor C;  C: high-order bit of the destination */
    pyint result, carry, testresult;
    if (B == 'B') {
        pyint bit15 = operand & 0x8000;
        pyint highbyte = operand & MASK_HIGH_BYTE;
        pyint bit7 = operand & 010000000;   /* Python: 0o0000000010000000 */
        pyint lowbyte = operand & MASK_LOW_BYTE;
        result = (py_shl(highbyte, 1) | py_shr(bit15, 7) | py_shl(lowbyte, 1) | py_shr(bit7, 7)) & MASK_WORD;
        carry = py_shr(bit15, 15) | py_shr(bit7, 7);
        testresult = result & MASK_LOW_BYTE;
    } else {
        pyint bit15 = operand & 0x8000;
        result = (py_shl(operand, 1) | py_shr(bit15, 15)) & MASK_WORD;
        carry = py_shr(bit15, 15);
        testresult = result;
    }
    pyint N = PSW_set_n(self->psw, B, testresult);
    pyint Z = PSW_set_z(self->psw, B, testresult);
    pyint C = carry & 01;
    pyint V = N ^ C;
    PSW_set_psw(self->psw, .n = N, .z = Z, .v = V, .c = C);
    char a[PYSTR], b[PYSTR], report[160];
    snprintf(report, sizeof report, "ROL(%s)=%s", py_bin(a, operand), py_bin(b, result));
    return ok(result, report);
}

InstrResult ASR(ss_ops *self, pyint operand, char B)
{
    /* 00 62 DD ASR arithmetic shift right.  Bit 15 is replicated.
     * v: N xor C;  c: low-order bit of the destination */
    pyint result, testresult, bit0;
    if (B == 'B') {
        pyint bit15 = operand & 0x8000;
        pyint highbyte = (operand & MASK_HIGH_BYTE) | 0400000;
        pyint bit7 = operand & 0x0080;
        pyint lowbyte = (operand & MASK_LOW_BYTE) | 0400000;
        bit0 = py_shr(operand & 0x0080, 8) | (operand & 0x0001);
        result = bit15 | (py_shr(highbyte, 1) & MASK_HIGH_BYTE) | bit7 | (py_shr(lowbyte, 1) & MASK_LOW_BYTE);
        testresult = result & MASK_LOW_BYTE;
    } else {
        operand = operand | 0400000;  /* set the high bit for python weirdness */
        pyint bit15 = operand & 0x8000;
        bit0 = operand & 0x0001;
        result = (py_shr(operand, 1) & MASK_WORD) | bit15;
        testresult = result;
    }
    pyint N = PSW_set_n(self->psw, B, testresult);
    pyint Z = PSW_set_z(self->psw, B, testresult);
    pyint C = bit0 & 01;
    pyint V = N ^ C;
    PSW_set_psw(self->psw, .n = N, .z = Z, .v = V, .c = C);
    return ok(result, "");
}

InstrResult ASL(ss_ops *self, pyint operand, char B)
{
    /* 00 63 DD ASL arithmetic shift left.  Bit 0 is loaded with 0.
     * v: N xor C;  c: high-order bit of the destination */
    pyint result, testresult, bit15;
    if (B == 'B') {
        pyint highbyte = operand & MASK_HIGH_BYTE;
        pyint lowbyte = operand & MASK_LOW_BYTE;
        bit15 = (operand & 01000000000000000LL) | py_shl(operand & 010000000, 8);  /* octal, as in the Python */
        /* Python is missing the operator between the halves (TypeError); | assumed */
        result = (py_shl(highbyte, 1) & MASK_HIGH_BYTE) | (py_shl(lowbyte, 1) & MASK_LOW_BYTE);
        testresult = result & MASK_LOW_BYTE;
    } else {
        bit15 = operand & 01000000000000000LL;   /* Python: 0o1000000000000000 */
        result = py_shl(operand, 1) & MASK_WORD;  /* take off that bit */
        testresult = result;
    }
    pyint N = PSW_set_n(self->psw, B, testresult);
    pyint Z = PSW_set_z(self->psw, B, testresult);
    pyint C = py_shr(bit15, 15);
    pyint V = N ^ C;
    PSW_set_psw(self->psw, .n = N, .z = Z, .v = V, .c = C);
    return ok(result, "");
}

InstrResult SXT(ss_ops *self, pyint operand, char B)
{
    /* 00 67 DD Sign Extend
     * (dst)<- 0 if n bit is clear;  (dst)<- -1 if n bit is set;  Z: set if n bit clear */
    pyint result, Z;
    if (PSW_get_n(self->psw) == 0) {
        result = 0;
        Z = 1;
    } else {
        result = 1;
        Z = PSW_get_z(self->psw);
    }
    PSW_set_psw(self->psw, .z = Z);
    result = ss_ops_byte_mask(self, B, result, operand);
    return ok(result, "");
}

/* standard PDP11 subroutine return; stack operation LSI11-03 */
InstrResult ss_MARK(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MARK **** not implemented"); }
/* move byte to PSW; LSI11-03 */
InstrResult MTPS(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MTPS **** not implemented"); }
/* Move from Previous Instruction Space; MMU instruction not in LSI11-03 */
InstrResult ss_MFPI(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MFPI NOT IMPLEMENTED"); }
/* move to previous instruction space; MMU instruction not in LSI11-03 */
InstrResult ss_MTPI(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MTPI NOT IMPLEMENTED"); }
/* Not PDP11-40 */
InstrResult MFPS(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MFPS NOT IMPLEMENTED"); }
/* 10 65 SS Move from previous data space */
InstrResult MFPD(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MFPD NOT IMPLEMENTED"); }
/* 10 66 SS Move to previous data space */
InstrResult MTPD(ss_ops *self, pyint operand, char B) { (void)self; (void)B; return ok(operand, "MTPD NOT IMPLEMENTED"); }

int ss_ops_is_ss_op(ss_ops *self, pyint instruction)
{
    /* 15  12 10  876 543 210
     *  * 000 101 *** *** ***
     *  * 000 110 *** *** ***
     * bit 15 can be 1 or 0;  bits 14,13,12 must be 0;  bits 11,10,9 must be 5 or 6 */
    (void)self;
    int bits_14_13_12 = (instruction & 0070000) == 0000000;
    pyint b11_9 = instruction & 0007000;
    int bits_11_10_9 = b11_9 == 0006000 || b11_9 == 0005000;
    int is_jmp = (instruction & 0177700) == 0000100;
    int is_swab = (instruction & 0177700) == 0000300;
    return (bits_14_13_12 && bits_11_10_9) || is_swab || is_jmp;
}

OpResult ss_ops_do_ss_op(ss_ops *self, pyint instruction)
{
    /* dispatch a single-operand opcode of form * 000 1** *** *** ***
     * 15-6 opcode (15 is 1 for a byte instruction, 0 for a word instruction)
     * 5-0 dst */
    StopWatches_start(self->sw, "ss");
    OpResult r = {0};

    char BW = ((instruction & 0100000) == 0100000) ? 'B' : 0;
    pyint source = instruction & 0000077;
    pyint opcode = instruction & 0107700;
    r.run = 1;

    ss_instruction fn;
    const char *name;
    if (!ss_ops_lookup(opcode, &fn, &name, NULL))
        PY_RAISE("KeyError", "single_operand_instruction_names");  /* raised outside the try in Python */

    if (opcode == 0000100) {
        /* special handling for JMP with R7. */
        AMJmp j = AddressModes_addressing_mode_jmp(self->am, source);
        r.run = j.run;
        snprintf(r.operand1, sizeof r.operand1, "%s", j.operand_word);
        snprintf(r.assembly, sizeof r.assembly, "%s %s", name, j.assembly);
        InstrResult ir = fn(self, j.jump_address, BW);
        snprintf(r.report, sizeof r.report, "%s", ir.report);
    } else {
        AMGet g = AddressModes_addressing_mode_get(self->am, BW, source);
        snprintf(r.operand1, sizeof r.operand1, "%s", g.operand_word);
        snprintf(r.assembly, sizeof r.assembly, "%s %s", name, g.assembly);
        InstrResult ir = fn(self, g.operand, BW);
        snprintf(r.report, sizeof r.report, "%s", ir.report);
        AddressModes_addressing_mode_set(self->am, BW, g.addressmode, ir.result, g.register_, g.address);
    }

    StopWatches_stop(self->sw, "ss");
    return r;
}
