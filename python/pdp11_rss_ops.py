"""pdp11_rss_ops.py double operand instructions: EIS, XOR, SOB

The instruction semantics follow the DEC processor handbook (and SIMH for
the DIV error flags), as in the C version in ../c; each FIX comment says what
the original Python did."""
import logging
import pdp11_util as u
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

class rss_ops:
    """Implements PDP11 double-operand RSS instructions"""
    def __init__(self, reg, ram, psw, am, sw):
        logging.info('initializing doubleOperandOps')
        self.reg = reg
        self.ram = ram
        self.psw = psw
        self.am = am
        self.sw = sw

        self.double_operand_RSS_instructions = {}
        self.double_operand_RSS_instructions[0o070000] = self.MUL
        self.double_operand_RSS_instructions[0o071000] = self.DIV
        self.double_operand_RSS_instructions[0o072000] = self.ASH
        self.double_operand_RSS_instructions[0o073000] = self.ASHC
        self.double_operand_RSS_instructions[0o074000] = self.XOR
        self.double_operand_RSS_instructions[0o077000] = self.SOB

        self.double_operand_RSS_instruction_names = {}
        self.double_operand_RSS_instruction_names[0o070000] = "MUL"
        self.double_operand_RSS_instruction_names[0o071000] = "DIV"
        self.double_operand_RSS_instruction_names[0o072000] = "ASH"
        self.double_operand_RSS_instruction_names[0o073000] = "ASHC"
        self.double_operand_RSS_instruction_names[0o074000] = "XOR"
        self.double_operand_RSS_instruction_names[0o077000] = "SOB"

    # ****************************************************
    # Double-Operand RSS instructions - 07 0R SS through 07 7R SS
    # ****************************************************

    # Where a register pair is used
    # (written below as "(Reg, Reg+1)",
    # the first register contains the low-order portion of the operand
    # and must be an even numbered register.
    # The next higher numbered register contains
    # the high-order portion of the operand (or the remainder).
    # An exception is the multiply instruction; Reg may be odd,
    # but if it is, the high 16 bits of the result are not stored.

    @staticmethod
    def sx16(value):
        """sign-extend a 16-bit PDP-11 value"""
        value &= MASK_WORD
        return value - 0o200000 if value & MASK_WORD_MSB else value

    @staticmethod
    def sx32(value):
        """sign-extend a 32-bit PDP-11 value"""
        value &= u.MASK_LONG
        return value - 0o40000000000 if value & u.MASK_LONG_MSB else value

    def MUL(self, rDest, source):
        """07 0R SS MUL 4-31

        (R, R|1) < R * (src)
        High word to R, low word to R|1 (for odd R, R receives the low word).
        N, Z on the 32-bit product; V cleared; C set if it does not fit in 16 bits.
        FIX: the Python never set C."""
        product = self.sx16(self.reg.get(rDest)) * self.sx16(source)
        self.reg.set(rDest, product >> 16)
        self.reg.set(rDest | 1, product)
        self.psw.set_psw(n=1 if product < 0 else 0, z=1 if product == 0 else 0, v=0,
                         c=1 if product > 0o77777 or product < -0o100000 else 0)
        return 0, ''

    def DIV(self, rDest, denominator):
        """07 1R SS DIV 4-32

        (R, R|1) < (R, R|1) / (src)
        Quotient to R, remainder (with the sign of the dividend) to R|1.
        Divide by zero: V and C set, registers unchanged.  Quotient overflow:
        V set, registers unchanged.  (Flags in the error cases follow SIMH.)
        FIX: the Python tested the numerator for zero, raised
        ZeroDivisionError on a zero denominator, and its quotient correction
        was wrong for exact negative divisions and for two negative operands."""
        numerator = (self.reg.get(rDest) << 16) | self.reg.get(rDest | 1)
        denominator &= MASK_WORD
        if denominator == 0:
            self.psw.set_psw(n=0, z=1, v=1, c=1)
            return 0, 'DIV divide by zero'
        n = self.sx32(numerator)
        d = self.sx16(denominator)
        # The PDP-11 truncates toward zero; Python's // rounds down.
        quotient = abs(n) // abs(d)
        if (n < 0) != (d < 0):
            quotient = -quotient
        if quotient > 0o77777 or quotient < -0o100000:
            self.psw.set_psw(n=1 if quotient < 0 else 0, z=0, v=1, c=0)
            return 0, 'DIV overflow'
        self.reg.set(rDest, quotient)
        self.reg.set(rDest | 1, n - d * quotient)
        self.psw.set_psw(n=1 if quotient < 0 else 0, z=1 if quotient == 0 else 0, v=0, c=0)
        return 0, ''

    @staticmethod
    def arith_shift(value, bits, count):
        """Shift a value of `bits` bits by count (the low 6 bits of the
        source, -32..31; negative shifts right).  Returns the result, V (the
        sign changed during a left shift) and C (the last bit shifted out)."""
        mask = (1 << bits) - 1
        sign = 1 << (bits - 1)
        n = count & 0o77
        v = 0
        c = 0
        value &= mask
        if n & 0o40:  # right shift by 64 - n
            for _ in range(0o100 - n):
                c = value & 1
                value = (value >> 1) | (value & sign)
        else:         # left shift by n
            for _ in range(n):
                c = 1 if value & sign else 0
                shifted = (value << 1) & mask
                if shifted & sign != value & sign:
                    v = 1
                value = shifted
        return value, v, c

    def ASH(self, rDest, source):
        """07 2R SS ASH arithmetic shift 4-33

        The shift count is the low 6 bits of the source, -32..+31; negative
        is a right shift.  V: the sign changed during the shift; C: the last
        bit shifted out.
        FIX: the Python took the sign from the register number and never set C."""
        result, v, c = self.arith_shift(self.reg.get(rDest), 16, source)
        self.reg.set(rDest, result)
        self.psw.set_psw(n=1 if result & MASK_WORD_MSB else 0, z=1 if result == 0 else 0, v=v, c=c)
        return 0, ''

    def ASHC(self, rDest, source):
        """07 3R SS ASHC arithmetic shift combined 4-34

        (R, R|1) are treated as one 32-bit value, high word in R.  For an odd
        R both halves are R, so a right shift rotates it.
        FIX: the Python shifted R right by the whole source value."""
        value = (self.reg.get(rDest) << 16) | self.reg.get(rDest | 1)
        result, v, c = self.arith_shift(value, 32, source)
        self.reg.set(rDest, result >> 16)
        self.reg.set(rDest | 1, result)
        self.psw.set_psw(n=1 if result & u.MASK_LONG_MSB else 0, z=1 if result == 0 else 0, v=v, c=c)
        return 0, ''

    def XOR(self, rDest, dest):
        """07 4R DD XOR 4-35

        (dst) < R ^ (dst); R unaffected.  N, Z; V cleared; C unaffected.
        do_rss_op reads R first and writes the result to dst.
        FIX: the Python wrote the result into R and did not clear V."""
        result = (self.reg.get(rDest) ^ dest) & MASK_WORD
        self.psw.set_psw(n=1 if result & MASK_WORD_MSB else 0, z=1 if result == 0 else 0, v=0)
        return result, ''

    def SOB(self, rDest, offset):
        """07 7R NN SOB sutract one and branch 4-63

        R < R - 1; if R != 0, PC < PC - 2 * NN.  Condition codes unaffected.
        FIX: the Python multiplied R by the operand."""
        result = (self.reg.get(rDest) - 1) & MASK_WORD
        self.reg.set(rDest, result)
        if result != 0:
            self.reg.set_pc(self.reg.get_pc() - 2 * (offset & 0o77), "SOB")
        return 0, ''

    def is_rss_op(self, instruction):
        """Using instruction bit pattern, determine whether it's an RSS RDD RNN instruction"""
        # 077R00 0 111 111 *** 000 000 SOB (jump & subroutine)
        # bit 15 = 0
        # bits 14-12 = 7
        # bits 9 10 11 in [0,1,2,3,4,7]
        bit15 = instruction & 0o100000 == 0o000000
        bits14_12 = instruction & 0o070000 == 0o070000
        bits11_9 = instruction & 0o077000 in [0o070000, 0o071000, 0o072000, 0o073000, 0o074000, 0o077000]
        return bit15 and bits14_12 and bits11_9

    def do_rss_op(self, instruction):
        """dispatch an RSS opcode"""
        # parameter: opcode of form 0 111 *** *** *** ***
        # register source
        # 15-9 opcode
        # 8-6 reg
        # 5-0 src
        self.sw.start("rss")
        opcode = instruction & 0o077000
        register = (instruction & 0o000700) >> 6
        source = instruction & 0o000077

        run = True
        name = self.double_operand_RSS_instruction_names[opcode]
        operand1 = ''
        # Each instruction stores its own results.
        # FIX: the Python fetched the source as a byte and stored every
        # result in R.
        if opcode == 0o077000:  # SOB R,NN
            assembly = f'{name} R{register},{oct(source)}'
            result, report = self.SOB(register, source)
        elif opcode == 0o074000:  # XOR R,DD
            # FIX: R is read before the destination is evaluated, so XOR
            # R1,(R1)+ uses R1's value from before the increment, as on the
            # 11/03.
            rvalue = self.reg.get(register)
            value, dest_register, dest_address, operand1, assembly1, dest_addressmode = self.am.addressing_mode_get('W', source)
            assembly = f'{name} R{register},{assembly1}'
            result = (rvalue ^ value) & MASK_WORD
            self.psw.set_psw(n=1 if result & MASK_WORD_MSB else 0, z=1 if result == 0 else 0, v=0)
            self.am.addressing_mode_set('W', dest_addressmode, result, dest_register, dest_address)
            report = ''
        else:  # MUL, DIV, ASH, ASHC SS,R
            value, source_register, source_address, operand1, assembly1, source_addressmode = self.am.addressing_mode_get('W', source)
            assembly = f'{name} {assembly1},R{register}'
            result, report = self.double_operand_RSS_instructions[opcode](register, value)
        self.sw.stop("rss")
        return run, operand1, "", assembly, report
