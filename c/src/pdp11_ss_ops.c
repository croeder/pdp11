/* pdp11_ss_ops.c - translation of pdp11_ss_ops.py (single operand instructions)
 *
 * The instruction semantics follow the DEC processor handbook rather than
 * the Python; each FIX comment says what the Python did.  The translation
 * that reproduces the Python exactly is tagged c-translation-faithful. */
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

/* Operand size helpers: M = value mask, S = sign bit */
#define SZ_M(B) ((B) == 'B' ? MASK_LOW_BYTE : MASK_WORD)
#define SZ_S(B) ((B) == 'B' ? MASK_BYTE_MSB : MASK_WORD_MSB)

/* set N and Z from a result already masked to the operand size */
static void set_nz(PSW *psw, char B, pyint r)
{
    PSW_set_psw(psw, .n = (r & SZ_S(B)) ? 1 : 0, .z = r == 0 ? 1 : 0);
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
    pyint d = operand & MASK_WORD;
    pyint result = py_shr(d & 0xFF00, 8) | py_shl(d & 0x00FF, 8);
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
    /* 00 51 DD Complement Destination.  N, Z; V cleared; C set */
    pyint r = ~operand & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = 0, .c = 1);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult INC(ss_ops *self, pyint operand, char B)
{
    /* 00 52 DD Increment Destination.  V set if (dst) was the largest
     * positive value; C unaffected.  FIX: the Python's V test wrote the mode bits. */
    pyint r = (operand + 1) & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = r == SZ_S(B) ? 1 : 0);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult DEC(ss_ops *self, pyint operand, char B)
{
    /* 00 53 DD Decrement Destination.  V set if (dst) was the most
     * negative value; C unaffected. */
    pyint r = (operand - 1) & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = r == (SZ_S(B) - 1) ? 1 : 0);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult NEG(ss_ops *self, pyint operand, char B)
{
    /* 00 54 DD Negate Destination.  V set if result is the most negative
     * value; C cleared if result is 0, set otherwise.
     * FIX: the Python stored a negative Python int and never set V or C. */
    pyint r = (-operand) & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = r == SZ_S(B) ? 1 : 0, .c = r != 0 ? 1 : 0);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult ADC(ss_ops *self, pyint operand, char B)
{
    /* 00 55 DD Add Carry.  FIX: the Python set no condition codes. */
    pyint c = PSW_get_c(self->psw);
    pyint r = (operand + c) & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = (c && r == SZ_S(B)) ? 1 : 0, .c = (c && r == 0) ? 1 : 0);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult SBC(ss_ops *self, pyint operand, char B)
{
    /* 00 56 DD Subtract Carry.  FIX: the Python set no condition codes. */
    pyint c = PSW_get_c(self->psw);
    pyint r = (operand - c) & SZ_M(B);
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = (c && r == SZ_S(B) - 1) ? 1 : 0, .c = (c && r == SZ_M(B)) ? 1 : 0);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult TST(ss_ops *self, pyint operand, char B)
{
    /* 00 57 DD Test Destination */
    set_nz(self->psw, B, operand & SZ_M(B));
    PSW_set_psw(self->psw, .v = 0, .c = 0);
    return ok(operand, "");
}

InstrResult ROR(ss_ops *self, pyint operand, char B)
{
    /* 00 60 DD ROR rotate right
     * Bit 0 is loaded into the C-bit and the previous carry into the sign bit.
     * N: sign of result;  Z: result = 0;  C: low-order bit of the destination;  V: N xor C
     * FIX: the Python byte version also rotated the high byte, and the word
     * version took C from the result instead of the old bit 0. */
    pyint d = operand & SZ_M(B);
    pyint c_in = PSW_get_c(self->psw);
    pyint r = py_shr(d, 1) | (c_in ? SZ_S(B) : 0);
    pyint C = d & 1;
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = PSW_get_n(self->psw) ^ C, .c = C);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult ROL(ss_ops *self, pyint operand, char B)
{
    /* 00 61 DD ROL rotate left
     * The sign bit is loaded into the carry and the previous carry into bit 0.
     * FIX: the Python ignored the carry and its byte version tested bit 21. */
    pyint d = operand & SZ_M(B);
    pyint c_in = PSW_get_c(self->psw);
    pyint r = (py_shl(d, 1) | c_in) & SZ_M(B);
    pyint C = (d & SZ_S(B)) ? 1 : 0;
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = PSW_get_n(self->psw) ^ C, .c = C);
    char a[PYSTR], b[PYSTR], report[160];
    snprintf(report, sizeof report, "ROL(%s)=%s", py_bin(a, operand), py_bin(b, r));
    return ok(ss_ops_byte_mask(self, B, r, operand), report);
}

InstrResult ASR(ss_ops *self, pyint operand, char B)
{
    /* 00 62 DD ASR arithmetic shift right.  The sign bit is replicated.
     * C: low-order bit of the destination;  V: N xor C */
    pyint d = operand & SZ_M(B);
    pyint r = py_shr(d, 1) | (d & SZ_S(B));
    pyint C = d & 1;
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = PSW_get_n(self->psw) ^ C, .c = C);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult ASL(ss_ops *self, pyint operand, char B)
{
    /* 00 63 DD ASL arithmetic shift left.  Bit 0 is loaded with 0.
     * C: the sign bit of the destination;  V: N xor C
     * FIX: the Python tested bit 45 for C (so C was always 0) and its byte
     * version raised TypeError. */
    pyint d = operand & SZ_M(B);
    pyint r = py_shl(d, 1) & SZ_M(B);
    pyint C = (d & SZ_S(B)) ? 1 : 0;
    set_nz(self->psw, B, r);
    PSW_set_psw(self->psw, .v = PSW_get_n(self->psw) ^ C, .c = C);
    return ok(ss_ops_byte_mask(self, B, r, operand), "");
}

InstrResult SXT(ss_ops *self, pyint operand, char B)
{
    /* 00 67 DD Sign Extend
     * (dst) <- 0 if N is clear, -1 (0o177777) if N is set
     * Z: set if N clear;  V: cleared;  N, C: unaffected
     * FIX: the Python stored 1 for -1 and did not clear V. */
    (void)B;
    (void)operand;
    pyint n = PSW_get_n(self->psw);
    PSW_set_psw(self->psw, .z = n ^ 1, .v = 0);
    return ok(n ? MASK_WORD : 0, "");
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
        if (j.run) {  /* FIX: an illegal JMP Rn halts without jumping to 0 */
            InstrResult ir = fn(self, j.jump_address, BW);
            snprintf(r.report, sizeof r.report, "%s", ir.report);
        }
    } else {
        /* FIX: CLR and SXT write without reading the destination; TST reads
         * without writing; the unimplemented stubs neither read nor write.
         * The Python read and wrote back every destination. */
        int unimplemented = fn == ss_MARK || fn == MTPS || fn == ss_MFPI || fn == ss_MTPI ||
                            fn == MFPS || fn == MFPD || fn == MTPD;
        int write_only = fn == CLR || fn == SXT;
        int read_only = fn == TST;
        AMGet g = (write_only || unimplemented) ? AddressModes_addressing_mode_address(self->am, BW, source)
                                                : AddressModes_addressing_mode_get(self->am, BW, source);
        snprintf(r.operand1, sizeof r.operand1, "%s", g.operand_word);
        snprintf(r.assembly, sizeof r.assembly, "%s %s", name, g.assembly);
        InstrResult ir = fn(self, g.operand, BW);
        snprintf(r.report, sizeof r.report, "%s", ir.report);
        if (!read_only && !unimplemented)
            AddressModes_addressing_mode_set(self->am, BW, g.addressmode, ir.result, g.register_, g.address);
    }

    StopWatches_stop(self->sw, "ss");
    return r;
}
