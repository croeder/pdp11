/* pdp11_ssdd_ops.c - translation of pdp11_ssdd_ops.py
 * Double-Operand SSDD instructions
 * 01 SS DD through 06 SS DD
 * 11 SS DD through 16 SS DD
 *
 * TRANSLATION NOTES
 * - CMP calls psw.set_v(BW, ...), which writes the mode bits (see
 *   pdp11_hardware.c), and returns dest so the destination is rewritten
 *   with its own value.  Reproduced.
 * - BIT computes byte_mask(BW, source, source & dest) and that result is
 *   written to the destination.  Reproduced. */
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

InstrResult MOV(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* 01 SS DD move 4-23   (dst) < (src) */
    pyint result = ssdd_ops_byte_mask(self, BW, source, dest);
    PSW_set_n(self->psw, BW, source);
    PSW_set_z(self->psw, BW, source);
    PSW_set_psw(self->psw, .v = 0);
    return ok(result, "");
}

InstrResult CMP(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* compare 4-24   (src)-(dst)
     * set the condition code based on that result but don't change the destination */
    pyint result = ssdd_ops_byte_mask(self, BW, source - dest, dest);
    PSW_set_n(self->psw, BW, result);
    PSW_set_z(self->psw, BW, result);
    PSW_set_v(self->psw, BW, result);
    return ok(dest, "");
}

InstrResult BIT(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit test 4-28   (src) ^ (dst)
     * Performs logical "and" comparison of the source and destination
     * and modifies condition codes accordingly. */
    pyint result = ssdd_ops_byte_mask(self, BW, source, source & dest);
    PSW_set_n(self->psw, BW, result);
    PSW_set_z(self->psw, BW, result);
    PSW_set_psw(self->psw, .v = 0);
    return ok(result, "");
}

InstrResult BIC(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit clear 4-29   (dst) < ~(src)&(dst)
     * for byte operations, low-order byte is affected, high-order byte is unchanged */
    pyint wsource = ~(source | 0400000) & MASK_WORD;
    pyint result = ssdd_ops_byte_mask(self, BW, wsource & dest, dest);
    PSW_set_n(self->psw, BW, result);
    PSW_set_z(self->psw, BW, result);
    PSW_set_psw(self->psw, .v = 0);
    return ok(result, "");
}

InstrResult BIS(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* bit set 4-30   (dst) < (src) v (dst) */
    pyint result = ssdd_ops_byte_mask(self, BW, source | dest, dest);
    PSW_set_n(self->psw, BW, result);
    PSW_set_z(self->psw, BW, result);
    PSW_set_psw(self->psw, .v = 0);
    return ok(result, "");
}

InstrResult ADDSUB(ssdd_ops *self, char BW, pyint source, pyint dest)
{
    /* 06 SS DD: ADD 4-25 (dst) < (src) + (dst)
     * 16 SS DD: SUB 4-26 (dst) < (dst) + ~(src) + 1
     * The ADD and SUB instructions use word addressing,
     * and have no byte-oriented variations.
     * N set if result < 0;  Z set if result = 0;  V on arithmetic overflow;
     * C: set if there was a carry from the most significant bit of the result */
    char a[PYSTR], b[PYSTR], c[PYSTR];
    logging_info("source:%s dest:%s", py_oct(a, source), py_oct(b, dest));
    pyint py_source = pythonifyPDP11Word(source);
    pyint py_dest = pythonifyPDP11Word(dest);
    pyint sS = ssdd_ops_sign(self, source);
    pyint sD = ssdd_ops_sign(self, dest);
    pyint v = 0;
    pyint cbit = 0;
    pyint pdp11_result;

    /* SUB is the "byte" version of ADD */
    if (BW == 'W') {  /* ADD */
        pyint py_result = py_source + py_dest;
        pdp11_result = PDP11WordifyPythonInteger(py_result);
        logging_info("py_source:%lld + py_dest:%lld = pdp11_result:%s",
                     (long long)py_source, (long long)py_dest, py_oct(c, pdp11_result));
        pyint sR = ssdd_ops_sign(self, pdp11_result);
        logging_info("sS:%lld sD:%lld sR:%lld", (long long)sS, (long long)sD, (long long)sR);
        if (source != 0 && dest != 0 && sS == sD) {
            if (sS != sR) {
                v = 1;
                cbit = 1;
            }
        }
        if (((source | dest) & MASK_WORD_MSB) == MASK_WORD_MSB && (pdp11_result & MASK_WORD_MSB) == 0) {
            logging_info("carry");
            cbit = 1;
        }
    } else {  /* SUB DST - SRC */
        pyint py_result = py_dest - py_source;
        pdp11_result = PDP11WordifyPythonInteger(py_result);
        logging_info("py_dest:%lld - py_source:%lld = py_result:%lld pdp11_result:%s",
                     (long long)py_dest, (long long)py_source, (long long)py_result, py_oct(c, pdp11_result));
        pyint sR = ssdd_ops_sign(self, pdp11_result);
        logging_info("sS:%lld sD:%lld sR:%lld", (long long)sS, (long long)sD, (long long)sR);
        if (source != 0 && dest != 0 && sS != sD) {
            if (sS == sR) {
                v = 1;
                cbit = 1;
            }
        }
    }

    PSW_set_n(self->psw, 'W', pdp11_result);
    PSW_set_z(self->psw, 'W', pdp11_result);
    PSW_set_psw(self->psw, .v = v, .c = cbit);
    return ok(pdp11_result, "");
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
    AMGet s = AddressModes_addressing_mode_get(self->am, bw, source);
    AMGet d = AddressModes_addressing_mode_get(self->am, bw, dest);

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

    AddressModes_addressing_mode_set(self->am, bw, d.addressmode, result, d.register_, d.address);
    StopWatches_stop(self->sw, "ssdd");

    snprintf(r.operand1, sizeof r.operand1, "%s", s.operand_word);
    snprintf(r.operand2, sizeof r.operand2, "%s", d.operand_word);
    return r;
}
