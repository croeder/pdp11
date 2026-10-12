"""pdp11_ss_ops.py single oprand instructions

The instruction semantics follow the DEC processor handbook, as in the C
version in ../c; each FIX comment says what the original Python did."""
import logging
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

class ss_ops:
    """Implements PDP11 single-operand ss instructions"""
    def __init__(self, reg, ram, psw, am, sw):
        logging.info('initializing SingleOperandOps')
        self.reg = reg
        self.ram = ram
        self.psw = psw
        self.am = am
        self.sw = sw

        # ****************************************************
        # Single-Operand instructions -
        # 00 50 DD - 00 77 DD
        # 10 50 DD - 10 77 DD
        # ****************************************************
        self.single_operand_instructions = {}
        self.single_operand_instructions[0o000100] = self.JMP
        self.single_operand_instructions[0o000300] = self.SWAB

        self.single_operand_instructions[0o005000] = self.CLR
        self.single_operand_instructions[0o005100] = self.COM
        self.single_operand_instructions[0o005200] = self.INC
        self.single_operand_instructions[0o005300] = self.DEC
        self.single_operand_instructions[0o005400] = self.NEG
        self.single_operand_instructions[0o005500] = self.ADC
        self.single_operand_instructions[0o005600] = self.SBC
        self.single_operand_instructions[0o005700] = self.TST
        self.single_operand_instructions[0o006000] = self.ROR
        self.single_operand_instructions[0o006100] = self.ROL
        self.single_operand_instructions[0o006200] = self.ASR
        self.single_operand_instructions[0o006300] = self.ASL
        self.single_operand_instructions[0o006400] = self.MARK
        self.single_operand_instructions[0o006500] = self.MFPI
        self.single_operand_instructions[0o006600] = self.MTPI
        self.single_operand_instructions[0o006700] = self.SXT

        self.single_operand_instructions[0o105000] = self.CLR  # CLRB
        self.single_operand_instructions[0o105100] = self.COM  # COMB
        self.single_operand_instructions[0o105200] = self.INC  # INCB
        self.single_operand_instructions[0o105300] = self.DEC  # DECB
        self.single_operand_instructions[0o105400] = self.NEG  # NEGB
        self.single_operand_instructions[0o105500] = self.ADC  # ADCB
        self.single_operand_instructions[0o105600] = self.SBC  # SBCB
        self.single_operand_instructions[0o105700] = self.TST  # TSTB
        self.single_operand_instructions[0o106000] = self.ROR  # RORB
        self.single_operand_instructions[0o106100] = self.ROL  # ROLB
        self.single_operand_instructions[0o106200] = self.ASR  # ASRB
        self.single_operand_instructions[0o106300] = self.ASL  # ASLB
        self.single_operand_instructions[0o106400] = self.MTPS
        self.single_operand_instructions[0o106500] = self.MFPD
        self.single_operand_instructions[0o106600] = self.MTPD
        self.single_operand_instructions[0o106700] = self.MFPS

        self.single_operand_instruction_names = {}
        self.single_operand_instruction_names[0o000100] = "JMP"
        self.single_operand_instruction_names[0o000300] = "SWAB"

        self.single_operand_instruction_names[0o005000] = "CLR"
        self.single_operand_instruction_names[0o005100] = "COM"
        self.single_operand_instruction_names[0o005200] = "INC"
        self.single_operand_instruction_names[0o005300] = "DEC"
        self.single_operand_instruction_names[0o005400] = "NEG"
        self.single_operand_instruction_names[0o005500] = "ADC"
        self.single_operand_instruction_names[0o005600] = "SBC"
        self.single_operand_instruction_names[0o005700] = "TST"
        self.single_operand_instruction_names[0o006000] = "ROR"
        self.single_operand_instruction_names[0o006100] = "ROL"
        self.single_operand_instruction_names[0o006200] = "ASR"
        self.single_operand_instruction_names[0o006300] = "ASL"
        self.single_operand_instruction_names[0o006400] = "MARK"
        self.single_operand_instruction_names[0o006500] = "MFPI"
        self.single_operand_instruction_names[0o006600] = "MTPI"
        self.single_operand_instruction_names[0o006700] = "SXT"

        self.single_operand_instruction_names[0o105000] = "CLRB"
        self.single_operand_instruction_names[0o105100] = "COMB"
        self.single_operand_instruction_names[0o105200] = "INCB"
        self.single_operand_instruction_names[0o105300] = "DECB"
        self.single_operand_instruction_names[0o105400] = "NEGB"
        self.single_operand_instruction_names[0o105500] = "ADCB"
        self.single_operand_instruction_names[0o105600] = "SBCB"
        self.single_operand_instruction_names[0o105700] = "TSTB"
        self.single_operand_instruction_names[0o106000] = "RORB"
        self.single_operand_instruction_names[0o106100] = "ROLB"
        self.single_operand_instruction_names[0o106200] = "ASRB"
        self.single_operand_instruction_names[0o106300] = "ASLB"
        self.single_operand_instruction_names[0o106400] = "MTPS"
        self.single_operand_instruction_names[0o106500] = "MFPD"
        self.single_operand_instruction_names[0o106600] = "MTPD"
        self.single_operand_instruction_names[0o106700] = "MFPS"

        self.single_operand_instruction_texts = {}
        self.single_operand_instruction_texts[0o000100] = "jump"
        self.single_operand_instruction_texts[0o000300] = "swap bytes"

        self.single_operand_instruction_texts[0o005000] = "clear destination"
        self.single_operand_instruction_texts[0o005100] = "complement"
        self.single_operand_instruction_texts[0o005200] = "increment"
        self.single_operand_instruction_texts[0o005300] = "decrement"
        self.single_operand_instruction_texts[0o005400] = "negative"
        self.single_operand_instruction_texts[0o005500] = "add carry"
        self.single_operand_instruction_texts[0o005600] = "subtract carry"
        self.single_operand_instruction_texts[0o005700] = "test"
        self.single_operand_instruction_texts[0o006000] = "rotate right"
        self.single_operand_instruction_texts[0o006100] = "rotate left"
        self.single_operand_instruction_texts[0o006200] = "arithmetic shift right"
        self.single_operand_instruction_texts[0o006300] = "arithmetic shift left"
        self.single_operand_instruction_texts[0o006400] = "subroutine cleanup"
        self.single_operand_instruction_texts[0o006500] = "move from previous instruction space"
        self.single_operand_instruction_texts[0o006600] = "move to previous instruction space"
        self.single_operand_instruction_texts[0o006700] = "sign extend"

        self.single_operand_instruction_texts[0o105000] = "clear destination byte"
        self.single_operand_instruction_texts[0o105100] = "complement byte"
        self.single_operand_instruction_texts[0o105200] = "increment byte"
        self.single_operand_instruction_texts[0o105300] = "decrement byte"
        self.single_operand_instruction_texts[0o105400] = "negative byte"
        self.single_operand_instruction_texts[0o105500] = "add carry byte"
        self.single_operand_instruction_texts[0o105600] = "subtract carry byte"
        self.single_operand_instruction_texts[0o105700] = "test byte"
        self.single_operand_instruction_texts[0o106000] = "rotate right byte"
        self.single_operand_instruction_texts[0o106100] = "rotate left byte"
        self.single_operand_instruction_texts[0o106200] = "arithmetic shift right byte"
        self.single_operand_instruction_texts[0o106300] = "arithmetic shift left byte"
        self.single_operand_instruction_texts[0o106400] = "move to PSW"
        self.single_operand_instruction_texts[0o106500] = "move from previous data space"
        self.single_operand_instruction_texts[0o106600] = "move to previous data space"
        self.single_operand_instruction_texts[0o106700] = "move from psw"

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
            #logging.info(f'                                  ; byte_mask(value:{bin(value)}, target:{bin(target)}) returns {bin(return_value)}')
        else:
            return_value = value
        return return_value

    def mask(self, value, B):
        """Apply the correct byte or word mask to the value"""
        result = 0o0
        if B == "B":
            result = value & MASK_LOW_BYTE
        else:
            result = value & MASK_WORD
        return result

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

    def JMP(self, operand, B):
        """00 01 DD JMP jump 4-56"""
        self.reg.set_pc(operand, 'JMP')
        return operand, ''

    def SWAB(self, operand, B):
        """00 03 DD Swap Bytes 4-17"""
        # Exchanges high-order byte and low-order byte of the destination word
        # n: set if high-order bit of low-order byte (bit 7) of result is set; cleared otherwise
        # z: set if low-order byte of result = 0; cleared otherwise
        # v: cleared
        # c: cleared
        d = operand & MASK_WORD
        result = ((d & 0xFF00) >> 8) | ((d & 0x00FF) << 8)
        self.psw.set_n("B", result & 0x00FF)
        self.psw.set_z("B", result & 0x00FF)
        self.psw.set_psw(v=0, c=0)
        return result, ''

    def CLR(self, operand, B):
        """00 50 DD Clear Destination"""
        result = self.byte_mask(B, 0, operand)
        self.psw.set_psw(n=0, z=1, v=0, c=0)
        return result, ''

    def COM(self, operand, B):
        """00 51 DD Complement Destination: N, Z; V cleared; C set"""
        result = ~operand & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=0, c=1)
        return self.byte_mask(B, result, operand), ''

    def INC(self, operand, B):
        """00 52 DD Increment Destination.  V set if (dst) was the largest
        positive value; C unaffected.
        FIX: the Python's V test wrote the mode bits."""
        result = (operand + 1) & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=1 if result == self.sign_bit(B) else 0)
        return self.byte_mask(B, result, operand), ''

    def DEC(self, operand, B):
        """00 53 DD Decrement Destination.  V set if (dst) was the most
        negative value; C unaffected.
        FIX: the Python's V test wrote the mode bits."""
        result = (operand - 1) & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=1 if result == self.sign_bit(B) - 1 else 0)
        return self.byte_mask(B, result, operand), ''

    def NEG(self, operand, B):
        """00 54 DD negate Destination.  V set if the result is the most
        negative value; C cleared if the result is 0, set otherwise.
        FIX: the Python stored a negative Python int and never set V or C."""
        result = -operand & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=1 if result == self.sign_bit(B) else 0, c=1 if result != 0 else 0)
        return self.byte_mask(B, result, operand), ''

    def ADC(self, operand, B):
        """00 55 DD Add Carry.  FIX: the Python set no condition codes."""
        c = self.psw.get_c()
        result = (operand + c) & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=1 if c and result == self.sign_bit(B) else 0, c=1 if c and result == 0 else 0)
        return self.byte_mask(B, result, operand), ''

    def SBC(self, operand, B):
        """00 56 DD Subtract Carry.  FIX: the Python set no condition codes."""
        c = self.psw.get_c()
        result = (operand - c) & self.size_mask(B)
        self.set_nz(B, result)
        self.psw.set_psw(v=1 if c and result == self.sign_bit(B) - 1 else 0,
                         c=1 if c and result == self.size_mask(B) else 0)
        return self.byte_mask(B, result, operand), ''

    def TST(self, operand, B):
        """00 57 DD Test Destination"""
        self.set_nz(B, operand & self.size_mask(B))
        self.psw.set_psw(v=0, c=0)
        return operand, ''

    def ROR(self, operand, B):
        """00 60 DD ROR rotate right.  Bit 0 goes to C and the previous carry
        to the sign bit.  N: sign of result; Z: result = 0; C: low-order bit
        of the destination; V: N xor C.
        FIX: the Python's byte version also rotated the high byte, and the
        word version took C from the result instead of the old bit 0."""
        d = operand & self.size_mask(B)
        result = (d >> 1) | (self.sign_bit(B) if self.psw.get_c() else 0)
        C = d & 1
        self.set_nz(B, result)
        self.psw.set_psw(v=self.psw.get_n() ^ C, c=C)
        return self.byte_mask(B, result, operand), ''

    def ROL(self, operand, B):
        """00 61 DD ROL rotate left.  The sign bit goes to C and the previous
        carry to bit 0.  V: N xor C.
        FIX: the Python ignored the carry and its byte version tested bit 21."""
        d = operand & self.size_mask(B)
        result = ((d << 1) | self.psw.get_c()) & self.size_mask(B)
        C = 1 if d & self.sign_bit(B) else 0
        self.set_nz(B, result)
        self.psw.set_psw(v=self.psw.get_n() ^ C, c=C)
        return self.byte_mask(B, result, operand), f'ROL({bin(operand)})={bin(result)}'

    def ASR(self, operand, B):
        """00 62 DD ASR arithmetic shift right.  The sign bit is replicated.
        C: low-order bit of the destination; V: N xor C."""
        d = operand & self.size_mask(B)
        result = (d >> 1) | (d & self.sign_bit(B))
        C = d & 1
        self.set_nz(B, result)
        self.psw.set_psw(v=self.psw.get_n() ^ C, c=C)
        return self.byte_mask(B, result, operand), ''

    def ASL(self, operand, B):
        """00 63 DD ASL arithmetic shift left.  Bit 0 is loaded with 0.
        C: the sign bit of the destination; V: N xor C.
        FIX: the Python tested bit 45 for C (so C was always 0) and its byte
        version raised TypeError."""
        d = operand & self.size_mask(B)
        result = (d << 1) & self.size_mask(B)
        C = 1 if d & self.sign_bit(B) else 0
        self.set_nz(B, result)
        self.psw.set_psw(v=self.psw.get_n() ^ C, c=C)
        return self.byte_mask(B, result, operand), ''

    def SXT(self, operand, B):
        """00 67 DD Sign Extend
        (dst) <- 0 if N is clear, -1 (0o177777) if N is set
        Z: set if N clear; V: cleared; N, C: unaffected
        FIX: the Python stored 1 for -1 and did not clear V."""
        n = self.psw.get_n()
        self.psw.set_psw(z=n ^ 1, v=0)
        return (MASK_WORD if n else 0), ''

    def MARK (self, operand, B):
        # standard PDP11 subroutine return
        # stack operation LSI11-03
        return operand, 'MARK **** not implemented'

    def MTPS (self, operand, B):
        # move byte to PSW
        # stack operation LSI11-03
        return operand, 'MTPS **** not implemented'

    def MFPI (self, operand, B):
        # Move from Previous Instruction Space
        # MMU instruction not in LSI11-03
        return operand, 'MFPI NOT IMPLEMENTED'

    def MTPI (self, operand, B):
        # move to previous instruction space
        # MMU instruction not in LSI11-03
        return operand, 'MTPI NOT IMPLEMENTED'

    def MFPS (self, operand, B):
        # Not PDP11-40
        # MMU instruction not in LSI11-03
        return operand, 'MFPS NOT IMPLEMENTED'

    def MFPD(self, operand, B):
        """10 65 SS Move from previous data space"""
        # MMU instruction not in LSI11-03
        return operand, 'MFPD NOT IMPLEMENTED'

    def MTPD(self, operand, B):
        """10 66 SS Move to previous data space"""
        # MMU instruction not in LSI11-03
        return operand, 'MTPD NOT IMPLEMENTED'

    def is_ss_op(self, instruction):
        """Using instruction bit pattern, determine whether it's a single-operand instruction"""
        # 15  12 10  876 543 210
        #  * 000 101 *** *** ***
        #  * 000 110 *** *** ***
        # bit 15 can be 1 or 0
        # bits 14,13,12 must be 0
        # bits 11,10,9 must be 5 or 6
        # bits 8,7,6 can be anything
        # bits 5-0 can be anything
        # 0o000301 is one of these
        # 0 000 000 101 *** ***
        bits_14_13_12 = instruction & 0o070000 == 0o000000
        bits_11_10_9 = instruction & 0o007000 in [0o006000, 0o005000]
        is_jmp = instruction & 0o177700 == 0o000100
        is_swab = instruction & 0o177700 == 0o000300
        return (bits_14_13_12 and bits_11_10_9) or is_swab or is_jmp

    def do_ss_op(self, instruction):
        """dispatch a single-operand opcode"""
        # parameter: opcode of form * 000 1** *** *** ***
        # single operands
        # 15-6 opcode
        # 15 is 1 to indicate a byte instruction
        # 15 is 0 to indicate a word instruction
        # 5-0 dst

        self.sw.start("ss")

        # is it a Byte or Word instruction?
        if (instruction & 0o100000) == 0o100000:
            BW = 'B'
        else:
            BW = ''
        source = instruction & 0o000077
        opcode = instruction & 0o107700
        run = True
        name = self.single_operand_instruction_names[opcode]
        if opcode == 0o000100:
            # special handling for JMP with R7.
            run, jump_address, operand, assembly = self.am.addressing_mode_jmp(source)
            assembly = f'{name} {assembly}'
            report = ''
            if run:  # FIX: an illegal JMP Rn halts without jumping to 0
                result, report = self.single_operand_instructions[opcode](jump_address, BW)
        else:
            # FIX: CLR and SXT write without reading the destination; TST reads
            # without writing; the unimplemented stubs neither read nor write.
            # The Python read and wrote back every destination.
            # MARK, MFPI, MTPI, MTPS, MFPD, MTPD, MFPS
            unimplemented = opcode in (0o006400, 0o006500, 0o006600, 0o106400, 0o106500, 0o106600, 0o106700)
            write_only = opcode in (0o005000, 0o105000, 0o006700)
            read_only = opcode in (0o005700, 0o105700)
            if write_only or unimplemented:
                get = self.am.addressing_mode_address
            else:
                get = self.am.addressing_mode_get
            source_value, out_register, out_address, operand, assembly, addressmode = get(BW, source)
            assembly = f'{name} {assembly}'
            result, report = self.single_operand_instructions[opcode](source_value, BW)
            if not read_only and not unimplemented:
                self.am.addressing_mode_set(BW, addressmode, result, out_register, out_address)

        self.sw.stop("ss")
        return run, operand, '', assembly, report
