/* pdp11_ssdd_ops.c - translation of pdp11_ssdd_ops.py
 * Double-Operand SSDD instructions
 * 01 SS DD through 06 SS DD
 * 11 SS DD through 16 SS DD
 *
 * The instruction semantics follow the DEC processor handbook rather than
 * the Python; each FIX comment says what the Python did.  The translation
 * that reproduces the Python exactly is tagged c-translation-faithful. */
#include "pdp11_ssdd_ops.h"
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

void ssdd_ops_init(ssdd_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw)
{
    logging_debug("initializing DoubleOperandOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->am = am;
    self->sw = sw;
}

ssdd_instruction ssdd_ops_instruction(pyint opcode)
{
    switch (opcode) {
    /* SSDD instructions with B variants the usual way */
    case 0010000: return MOV;
    case 0020000: return CMP;
    case 0030000: return BIT;
    case 0040000: return BIC;
    case 0050000: return BIS;
    /* SSDD instructions that break the rule */
    case 0060000: return ADDSUB;
    default: return NULL;
    }
}

const char *ssdd_ops_name(pyint name_opcode)
{
    switch (name_opcode) {
    case 0010000: return "MOV";
    case 0020000: return "CMP";
    case 0030000: return "BIT";
    case 0040000: return "BIC";
    case 0050000: return "BIS";
    case 0060000: return "ADD";
    case 0110000: return "MOVB";
    case 0120000: return "CMPB";
    case 0130000: return "BITB";
    case 0140000: return "BICB";
    case 0150000: return "BISB";
    case 0160000: return "SUB";
    default: return NULL;
    }
}

pyint ssdd_ops_sign(ssdd_ops *self, pyint a)
{
    (void)self;
    return ((py_abs(a) & MASK_WORD_MSB) == MASK_WORD_MSB) ? -1 : 1;
}

pyint ssdd_ops_byte_mask(ssdd_ops *self, char BW, pyint value, pyint target)
{
    /* For byte operations only the low byte is affected; the target high byte remains. */
    (void)self;
    if (BW == 'B')
        return (value & MASK_LOW_BYTE) | (target & MASK_HIGH_BYTE);
    return value;
}

#define SZ_M(B) ((B) == 'B' ? MASK_LOW_BYTE : MASK_WORD)
#define SZ_S(B) ((B) == 'B' ? MASK_BYTE_MSB : MASK_WORD_MSB)

static void set_nz(PSW *psw, char B, pyint r)
{
    PSW_set_psw(psw, .n = (r & SZ_S(B)) ? 1 : 0, .z = r == 0 ? 1 : 0);
}

InstrResult MOV(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* 01 SS DD move 4-23   (dst) < (src).  N, Z; V cleared; C unaffected.
     * MOVB to a register sign-extends into the high byte.
     * FIX: the Python kept the register's old high byte. */
    (void)self;
    (void)dest;
    pyint s = source & SZ_M(BW);
    set_nz(self->psw, BW, s);
    PSW_set_psw(self->psw, .v = 0);
    if (BW == 'B' && (s & MASK_BYTE_MSB))
        return ok(s | MASK_HIGH_BYTE, "");   /* do_ssdd_op writes only the low byte to memory */
    return ok(s, "");
}

InstrResult CMP(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* compare 4-24   (src)-(dst); neither operand changes.
     * V: operands of opposite sign and the result has the sign of dst
     * C: set on a borrow, i.e. src < dst unsigned
     * FIX: the Python's V test wrote the mode bits and C was never set. */
    pyint s = source & SZ_M(BW), d = dest & SZ_M(BW);
    pyint r = (s - d) & SZ_M(BW);
    set_nz(self->psw, BW, r);
    PSW_set_psw(self->psw, .v = ((s ^ d) & (~d ^ r) & SZ_S(BW)) ? 1 : 0, .c = s < d ? 1 : 0);
    return ok(dest, "");
}

InstrResult BIT(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit test 4-28   (src) & (dst); neither operand changes.
     * FIX: the Python tested byte_mask(source, source & dest) and wrote it to dst. */
    pyint r = source & dest & SZ_M(BW);
    set_nz(self->psw, BW, r);
    PSW_set_psw(self->psw, .v = 0);
    return ok(dest, "");
}

InstrResult BIC(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit clear 4-29   (dst) < ~(src)&(dst).  For bytes only the low byte changes. */
    pyint r = ~source & dest & SZ_M(BW);
    set_nz(self->psw, BW, r);
    PSW_set_psw(self->psw, .v = 0);
    return ok(ssdd_ops_byte_mask(self, BW, r, dest), "");
}

InstrResult BIS(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit set 4-30   (dst) < (src) | (dst) */
    pyint r = (source | dest) & SZ_M(BW);
    set_nz(self->psw, BW, r);
    PSW_set_psw(self->psw, .v = 0);
    return ok(ssdd_ops_byte_mask(self, BW, r, dest), "");
}

InstrResult ADDSUB(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* 06 SS DD: ADD 4-25 (dst) < (src) + (dst)
     * 16 SS DD: SUB 4-26 (dst) < (dst) - (src)
     * Word-only; SUB is encoded where a "byte ADD" would be.
     * ADD V: operands of the same sign and the result of the other sign;
     *     C: carry out of bit 15.
     * SUB V: operands of opposite sign and the result has the sign of src;
     *     C: set on a borrow, i.e. src > dst unsigned.
     * FIX: the Python tied C to V, missed carries, and never set C on SUB borrows. */
    pyint s = source & MASK_WORD, d = dest & MASK_WORD, r, v, c;
    if (BW == 'W') {  /* ADD */
        r = (s + d) & MASK_WORD;
        v = (~(s ^ d) & (s ^ r) & MASK_WORD_MSB) ? 1 : 0;
        c = (s + d) > MASK_WORD ? 1 : 0;
    } else {          /* SUB */
        r = (d - s) & MASK_WORD;
        v = ((s ^ d) & (~s ^ r) & MASK_WORD_MSB) ? 1 : 0;
        c = s > d ? 1 : 0;
    }
    set_nz(self->psw, 'W', r);
    PSW_set_psw(self->psw, .v = v, .c = c);
    return ok(r, "");
}

int ssdd_ops_is_ssdd_op(ssdd_ops *self, pyint instruction)
{
    /* bits 14 - 12 in [1, 2, 3, 4, 5, 6] */
    (void)self;
    pyint b = instruction & 0070000;
    return b == 0010000 || b == 0020000 || b == 0030000 || b == 0040000 || b == 0050000 || b == 0060000;
}

OpResult ssdd_ops_do_ssdd_op(ssdd_ops *self, pyint instruction)
{
    /* dispatch a double-operand opcode of form * +++ *** *** *** ***
     * where +++ = not 000 and not 111
     * 15-12 opcode, 11-6 src, 5-0 dst */
    StopWatches_start(self->sw, "ssdd");
    OpResult r = {0};
    char bw = (py_shr(instruction & 0100000, 15) == 1) ? 'B' : 'W';
    pyint opcode = instruction & 0070000;
    pyint name_opcode = instruction & 0170000;

    pyint source = py_shr(instruction & 0007700, 6);
    pyint dest = instruction & 0000077;
    /* ADD and SUB are word instructions even though SUB has bit 15 set.
     * FIX: the Python fetched SUB's operands as bytes. */
    char size = (opcode == 0060000) ? 'W' : bw;
    AMGet s = AddressModes_addressing_mode_get(self->am, size, source);
    /* FIX: MOV does not read its destination (the Python did, which matters
     * for device registers such as the DL11 receive buffer). */
    AMGet d = (opcode == 0010000) ? AddressModes_addressing_mode_address(self->am, size, dest)
                                  : AddressModes_addressing_mode_get(self->am, size, dest);

    r.run = 1;
    pyint result;
    ssdd_instruction fn = ssdd_ops_instruction(opcode);
    const char *name = ssdd_ops_name(name_opcode);
    if (fn && name) {
        InstrResult ir = fn(self, bw, s.operand, d.operand);
        result = ir.result;
        snprintf(r.report, sizeof r.report, "%s", ir.report);
        snprintf(r.assembly, sizeof r.assembly, "%s %s,%s", name, s.assembly, d.assembly);
    } else {
        /* Python: result = False, then `report` is unbound at the return
         * (UnboundLocalError) */
        PY_RAISE("UnboundLocalError", "report referenced before assignment");
    }

    /* FIX: CMP and BIT do not write their destination. */
    if (opcode != 0020000 && opcode != 0030000)
        AddressModes_addressing_mode_set(self->am, size, d.addressmode, result, d.register_, d.address);
    StopWatches_stop(self->sw, "ssdd");

    snprintf(r.operand1, sizeof r.operand1, "%s", s.operand_word);
    snprintf(r.operand2, sizeof r.operand2, "%s", d.operand_word);
    return r;
}
