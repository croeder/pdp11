"""pdp11_ssdd_ops.py double operand instructions

The instruction semantics follow the DEC processor handbook, as in the C
version in ../c; each FIX comment says what the original Python did."""
import logging
import pdp11_util as u
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

class ssdd_ops:
    """Implements PDP11 double-operand ssdd instructions"""
    def __init__(self, reg, ram, psw, am, sw):
        logging.debug('initializing DoubleOperandOps')
        self.reg = reg
        self.ram = ram
        self.psw = psw
        self.am = am
        self.sw = sw

        self.double_operand_SSDD_instructions = {}
        # SSDD instructions with B variants the usual way
        self.double_operand_SSDD_instructions[0o010000] = self.MOV
        self.double_operand_SSDD_instructions[0o020000] = self.CMP
        self.double_operand_SSDD_instructions[0o030000] = self.BIT
        self.double_operand_SSDD_instructions[0o040000] = self.BIC
        self.double_operand_SSDD_instructions[0o050000] = self.BIS

        # SSDD instructions that break the rule
        self.double_operand_SSDD_instructions[0o060000] = self.ADDSUB

        self.double_operand_SSDD_instruction_names = {}
        self.double_operand_SSDD_instruction_names[0o010000] = "MOV"
        self.double_operand_SSDD_instruction_names[0o020000] = "CMP"
        self.double_operand_SSDD_instruction_names[0o030000] = "BIT"
        self.double_operand_SSDD_instruction_names[0o040000] = "BIC"
        self.double_operand_SSDD_instruction_names[0o050000] = "BIS"
        self.double_operand_SSDD_instruction_names[0o060000] = "ADD"
        self.double_operand_SSDD_instruction_names[0o110000] = "MOVB"
        self.double_operand_SSDD_instruction_names[0o120000] = "CMPB"
        self.double_operand_SSDD_instruction_names[0o130000] = "BITB"
        self.double_operand_SSDD_instruction_names[0o140000] = "BICB"
        self.double_operand_SSDD_instruction_names[0o150000] = "BISB"
        self.double_operand_SSDD_instruction_names[0o160000] = "SUB"

    # ****************************************************
    # Double-Operand SSDD instructions
    # 01 SS DD through 06 SS DD
    # 11 SS DD through 16 SS DD
    # ****************************************************

    def sign(self, a):
        if abs(a) & MASK_WORD_MSB == MASK_WORD_MSB:
            return -1
        else:
            return 1

    def byte_mask(self, BW, value, target):
        """mask source and result like PDP11 does
        @param BW:
        @param value: new value to stuff into the return value
        @param target: resgiter where it's going
        @return: result with maybe old high byte
        """
        if BW == 'B':
            # Only make the operation affect the low byte
            # The target high byte remains unnafected
            return_value = (value & MASK_LOW_BYTE) | (target & MASK_HIGH_BYTE)
            #logging.debug(f'                                  ; byte_mask(value:{bin(value)}, target:{bin(target)}) returns {bin(return_value)}')
        else:
            return_value = value
        return return_value

    # operand size helpers: the value mask and the sign bit
    @staticmethod
    def size_mask(B):
        return MASK_LOW_BYTE if B == 'B' else MASK_WORD

    @staticmethod
    def sign_bit(B):
        return MASK_BYTE_MSB if B == 'B' else MASK_WORD_MSB

    def set_nz(self, B, result):
        """set N and Z from a result already masked to the operand size"""
        self.psw.set_psw(n=1 if result & self.sign_bit(B) else 0, z=1 if result == 0 else 0)

    def MOV(self, BW, source, dest):
        """01 SS DD move 4-23

        (dst) < (src).  N, Z; V cleared; C unaffected.
        MOVB to a register sign-extends into the high byte; do_ssdd_op writes
        only the low byte to memory.
        FIX: the Python kept the register's old high byte."""
        s = source & self.size_mask(BW)
        self.set_nz(BW, s)
        self.psw.set_psw(v=0)
        if BW == 'B' and s & MASK_BYTE_MSB:
            return s | MASK_HIGH_BYTE, ''
        return s, ''

    def CMP(self, BW, source, dest):
        """compare 4-24
        (src)-(dst); neither operand changes.
        V: operands of opposite sign and the result has the sign of dst.
        C: set on a borrow, that is src < dst unsigned.
        FIX: the Python's V test wrote the mode bits and C was never set."""
        s = source & self.size_mask(BW)
        d = dest & self.size_mask(BW)
        result = (s - d) & self.size_mask(BW)
        self.set_nz(BW, result)
        self.psw.set_psw(v=1 if (s ^ d) & (~d ^ result) & self.sign_bit(BW) else 0, c=1 if s < d else 0)
        return dest, ''

    def BIT(self, BW, source, dest):
        """bit test 4-28

        (src) & (dst); neither operand changes.
        FIX: the Python tested byte_mask(source, source & dest) and wrote it
        to dst."""
        result = source & dest & self.size_mask(BW)
        self.set_nz(BW, result)
        self.psw.set_psw(v=0)
        return dest, ''

    def BIC(self, BW, source, dest):
        """bit clear 4-29

        (dst) < ~(src)&(dst).  For bytes only the low byte changes."""
        result = ~source & dest & self.size_mask(BW)
        self.set_nz(BW, result)
        self.psw.set_psw(v=0)
        return self.byte_mask(BW, result, dest), ''

    def BIS(self, BW, source, dest):
        """bit set 4-30
        (dst) < (src) | (dst)"""
        result = (source | dest) & self.size_mask(BW)
        self.set_nz(BW, result)
        self.psw.set_psw(v=0)
        return self.byte_mask(BW, result, dest), ''

    def ADDSUB(self, BW, source, dest):
        """06 SS DD: ADD 4-25 (dst) < (src) + (dst)
        | 16 SS DD: SUB 4-26 (dst) < (dst) - (src)
        The ADD and SUB instructions use word addressing,
        and have no byte-oriented variations.

        ADD V: operands of the same sign and the result of the other sign;
            C: carry out of bit 15.
        SUB V: operands of opposite sign and the result has the sign of src;
            C: set on a borrow, that is src > dst unsigned.
        FIX: the Python tied C to V, missed carries, and never set C on SUB
        borrows.
        """
        s = source & MASK_WORD
        d = dest & MASK_WORD
        # SUB is the "byte" version of ADD
        if BW == 'W':  # ADD
            result = (s + d) & MASK_WORD
            v = 1 if ~(s ^ d) & (s ^ result) & MASK_WORD_MSB else 0
            c = 1 if s + d > MASK_WORD else 0
        else:  # SUB DST - SRC
            result = (d - s) & MASK_WORD
            v = 1 if (s ^ d) & (~s ^ result) & MASK_WORD_MSB else 0
            c = 1 if s > d else 0
        self.set_nz('W', result)
        self.psw.set_psw(v=v, c=c)
        return result, ''

    def is_ssdd_op(self, instruction):
        """Using instruction bit pattern, determine whether it's a souble operand instruction"""
        # bits 14 - 12 in [1, 2, 3, 4, 5, 6]
        #logging.debug(f'is_ssdd_op {oct(instruction)}&0o070000={instruction & 0o070000}')
        bits14_12 = instruction & 0o070000 in [0o010000, 0o020000, 0o030000, 0o040000, 0o050000, 0o060000]
        return bits14_12

    def do_ssdd_op(self, instruction):
        """dispatch a double-operand opcode.
        parameter: opcode of form * +++ *** *** *** ***
        where +++ = not 000 and not 111 and not 110 """
        # double operands
        # 15-12 opcode
        # 11-6 src
        # 5-0 dst

        #        * +++ *** *** *** *** double operands
        # •1SSDD * 001 *** *** *** *** MOV move source to destination (double)
        # •2SSDD * 010 *** *** *** *** CMP compare src to dst (double)
        # •3SSDD * 011 *** *** *** *** BIT bit test (double)
        # •4SSDD * 100 *** *** *** *** BIC bit clear (double)
        # •5SSDD * 101 *** *** *** *** BIS bit set (double)

        self.sw.start("ssdd")
        if (instruction & 0o100000) >> 15 == 1:
            bw = 'B'
        else:
            bw = 'W'
        opcode = instruction & 0o070000
        name_opcode = instruction & 0o170000

        source = (instruction & 0o007700) >> 6
        dest = instruction & 0o000077
        #logging.debug(f'    source_value  = addressing_mode_get')
        # ADD and SUB are word instructions even though SUB has bit 15 set.
        # FIX: the Python fetched SUB's operands as bytes.
        size = 'W' if opcode == 0o060000 else bw
        source_value, source_register, source_address, operand1, assembly1, source_addressmode = self.am.addressing_mode_get(size, source)
        # FIX: MOV does not read its destination (the Python did, which
        # matters for device registers such as the DL11 receive buffer).
        if opcode == 0o010000:
            get = self.am.addressing_mode_address
        else:
            get = self.am.addressing_mode_get
        dest_value, dest_register, dest_address, operand2, assembly2, dest_addressmode = get(size, dest)

        run = True
        result, report = self.double_operand_SSDD_instructions[opcode](bw, source_value, dest_value)
        assembly = f'{self.double_operand_SSDD_instruction_names[name_opcode]} {assembly1},{assembly2}'

        # FIX: CMP and BIT do not write their destination.
        if opcode not in (0o020000, 0o030000):
            self.am.addressing_mode_set(size, dest_addressmode, result, dest_register, dest_address)
        self.sw.stop("ssdd")

        return run, operand1, operand2, assembly, report
