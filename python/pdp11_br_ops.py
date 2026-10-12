"""pdp11_br_ops.py branch instructions"""
import logging

# masks for accessing words and bytes
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200

# MASK_HIGH_BYTE - opcode
# MASK_LOW_BYTE - offset
# extend sign of offset through bits 8-15
# multiply by two
# add to PC to make branch address

class br_ops:
    """Implements PDP11 branch operations"""
    def __init__(self, reg, ram, psw, sw):
        logging.info('initializing branchOps')
        self.reg = reg
        self.ram = ram
        self.psw = psw
        self.sw = sw

        self.branch_instructions = {}
        self.branch_instructions[0o000400] = self.BR
        self.branch_instructions[0o001000] = self.BNE
        self.branch_instructions[0o001400] = self.BEQ
        self.branch_instructions[0o002000] = self.BGE
        self.branch_instructions[0o002400] = self.BLT
        self.branch_instructions[0o003000] = self.BGT
        self.branch_instructions[0o003400] = self.BLE
        self.branch_instructions[0o100000] = self.BPL
        self.branch_instructions[0o100400] = self.BMI
        self.branch_instructions[0o101000] = self.BHI
        self.branch_instructions[0o101400] = self.BLOS
        self.branch_instructions[0o102000] = self.BVC
        self.branch_instructions[0o102400] = self.BVS
        self.branch_instructions[0o103000] = self.BCC  # BHIS
        self.branch_instructions[0o103400] = self.BCS  # BLO

        self.branch_instruction_names = {}
        self.branch_instruction_names[0o000400] = "BR"
        self.branch_instruction_names[0o001000] = "BNE"
        self.branch_instruction_names[0o001400] = "BEQ"
        self.branch_instruction_names[0o002000] = "BGE"
        self.branch_instruction_names[0o002400] = "BLT"
        self.branch_instruction_names[0o003000] = "BGT"
        self.branch_instruction_names[0o003400] = "BLE"
        self.branch_instruction_names[0o100000] = "BPL"
        self.branch_instruction_names[0o100400] = "BMI"
        self.branch_instruction_names[0o101000] = "BHI"
        self.branch_instruction_names[0o101400] = "BLOS"
        self.branch_instruction_names[0o102000] = "BVC"
        self.branch_instruction_names[0o102400] = "BVS"
        self.branch_instruction_names[0o103000] = "BCC"  # BCC = BHIS
        self.branch_instruction_names[0o103400] = "BCS"  # BCS = BLO


    # ****************************************************
    # Branch instructions
    # 00 04 XXX - 00 34 XXX
    # 10 00 XXX - 10 34 XXX
    # ****************************************************
    # Symbols in DEC book and Python operators
    # A = boolean AND = &
    # get_v = boolean OR = |
    # -get_v = exclusive OR = ^
    # ~ = boolean NOT = ~
    # ****************************************************

    def BR(self, offset):
        # logging.info(f'    BR instruction:{oct(instruction)} offset:{oct(offset)}')
        """00 04 XXX Branch"""
        self.reg.set_pc_2x_offset(offset, "BR")
        return True
        # with the Branch instruction at location 500 see p. 4-37

    def BNE(self, offset):
        """00 10 XXX branch if not equal get_z=0"""
        # Tests the state of the z-bit.
        # If the z-bit is clear, branches.
        #logging.info(f"    BNE get_z:{self.psw.get_z()}")
        if self.psw.get_z() == 0:
            self.reg.set_pc_2x_offset(offset, "BNE")
        return True

    def BEQ(self, offset):
        """00 14 XXX branch if equal get_z=1"""
        # Tests the state of the get_z-bit and causes a branch if get_z is set
        #logging.info(f"    BEQ get_z:{self.psw.get_z()}")
        if self.psw.get_z() == 1:
            self.reg.set_pc_2x_offset(offset, "BEQ")
        return True

    def BGE(self, offset):
        """00 20 XXX branch if greater than or equal 4-47: N xor V = 0
        FIX: the Python tested N or V."""
        if self.psw.get_n() ^ self.psw.get_v() == 0:
            self.reg.set_pc_2x_offset(offset, "BGE")
        return True

    def BLT(self, offset):
        """"00 24 XXX branch if less thn zero"""
        if self.psw.get_n() ^ self.psw.get_v() == 1:
            self.reg.set_pc_2x_offset(offset, "BLT")
        return True

    def BGT(self, offset):
        """00 30 XXX branch if equal get_z=1"""
        if self.psw.get_z() | (self.psw.get_n() ^ self.psw.get_v()) == 0:
            self.reg.set_pc_2x_offset(offset, "BTG")
        return True

    def BLE(self, offset):
        """00 34 XXX branch if equal get_z=1"""
        if self.psw.get_z() | (self.psw.get_n() ^ self.psw.get_v()) == 1:
            self.reg.set_pc_2x_offset(offset, "BLE")
        return True

    def BPL(self, offset):
        """01 00 XXX branch if positive get_n=0"""
        if self.psw.get_n() == 0:
            self.reg.set_pc_2x_offset(offset, 'BPL')
        return True

    def BMI(self, offset):
        """10 04 XXX branch if negative get_n=1"""
        if self.psw.get_n() == 1:
            self.reg.set_pc_2x_offset(offset, 'BMI')
        return True

    def BHI(self, offset):
        """10 10 XXX branch if higher"""
        if self.psw.get_c() == 0 and self.psw.get_z() == 0:
            self.reg.set_pc_2x_offset(offset, 'BHI')
        return True

    def BLOS(self, offset):
        """10 14 XXX branch if lower or same"""
        if self.psw.get_c() | self.psw.get_z() == 1:
            self.reg.set_pc_2x_offset(offset, 'BLOS')
        return True

    def BVC(self, offset):
        """10 20 XXX Branch if overflow is clear get_v=0"""
        if self.psw.get_v() == 0:
            self.reg.set_pc_2x_offset(offset, 'BVC')
        return True

    def BVS(self, offset):
        """10 24 XXX Branch if overflow is set get_v=1"""
        if self.psw.get_v() == 1:
            self.reg.set_pc_2x_offset(offset, 'BVS')
        return True

    def BCC(self, offset):
        """10 30 XXX branch if Carry Clear, BHIS is the sme as BCC"""
        if self.psw.get_c() == 0:
            self.reg.set_pc_2x_offset(offset, 'BCC')
        return True

    def BCS(self, offset):
        """10 34 XXX branch if lower. BCS is the same as BLO"""
        if self.psw.get_c() == 1:
            self.reg.set_pc_2x_offset(offset, 'BCS')
        return True

    def is_br_op(self, instruction):
        """Using instruction bit pattern, determine whether it's a branch instruction"""
        # *0 ** xxx
        # bit 15 can be 1 or 0; mask = 0o100000
        # bits 14-12 = 0; mask = 0o070000
        # bit 11,10,9,8; mask = 0o007400
        # bits 7,6, 5,4,3, 2,1,0 are the offset; mask = 0o000377
        blankbits = instruction & 0o070000 == 0o000000
        lowbits0 = instruction & 0o107400 in [          0o000400, 0o001000, 0o001400, 0o002000, 0o002400, 0o003000, 0o003400]
        lowbits1 = instruction & 0o107400 in [0o100000, 0o100400, 0o101000, 0o101400, 0o102000, 0o102400, 0o103000, 0o103400]
        # logging.info(f'{instruction} {blankbits} and ({lowbits0} or {lowbits1})')
        return blankbits and (lowbits0 or lowbits1)

    def do_br_op(self, instruction):
        """dispatch a branch opcode"""
        #parameter: opcode of form X 000 0XX X** *** ***
        self.sw.start("br")
        opcode = instruction & MASK_HIGH_BYTE
        offset = instruction & MASK_LOW_BYTE
        try:
            assembly = f'{self.branch_instruction_names[opcode]} {oct(offset)}' #     ; branch offset {oct(2*offset+2)}'
            result = self.branch_instructions[opcode](offset)
        except KeyError:
            assembly = 'Error: branch opcode not found'
            result = False

        self.sw.stop("br")
        return result, '', '', assembly, ''
