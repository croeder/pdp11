"""pdp11other - other instructions"""
import logging
from pdp11_hardware import Stack

# masks for accessing words and bytes
MASK_BYTE = 0o000377
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200

class other_ops:
    """Implements the remainder of PDP11 instructions"""
    def __init__(self, reg, ram, psw, am, sw):
        logging.info('initializing OtherOps')
        self.reg = reg
        self.ram = ram
        self.psw = psw
        self.am = am
        self.stack = Stack(reg, ram, psw)
        self.sw = sw

        self.other_instructions = {}
        self.other_instructions[0o000200] = self.RTS
        self.other_instructions[0o004000] = self.JSR
        self.other_instructions[0o006400] = self.MARK
        self.other_instructions[0o006500] = self.MFPI
        self.other_instructions[0o006600] = self.MTPI

    # ****************************************************
    # Other instructions
    # ****************************************************
    def RTS(self, instruction):
        """00 20 0R: RTS return from subroutine 4-60

        | PC <- reg
        | reg <- (SP)^
        """
        R = instruction & 0o000007
        self.reg.set_pc(self.reg.get(R), "RTS")
        self.reg.set(R, self.stack.pop())
        return True, 'RTS', f'    {oct(self.reg.get_pc())} {oct(instruction)} RTS R{R}'

    def JSR(self, instruction):
        """00 4R DD: JSR jump to subroutine 4-58

        |  pushstack(reg)
        |  reg <- PC+2
        |  PC <- (dst)
        """
        R = (instruction & 0o000700) >> 6
        DD = instruction & 0o000077
        run, jump_address, operand_word, assembly = self.am.addressing_mode_jmp(DD)
        if not run:
            # FIX: JSR R,Rn is illegal; stop instead of jumping to 0 (the Python
            # pushed R and jumped to 0).  *** should trap
            return False, 'JSR', 'JSR with register destination is illegal'
        # FIX: SP is decremented before R is read, so JSR SP,dst pushes the
        # decremented SP, as SIMH and the hardware do (the Python pushed the
        # old SP).
        self.reg.set_sp(self.reg.get_sp() - 2, "JSR")
        self.ram.write_word(self.reg.get_sp(), self.reg.get(R))
        self.reg.set(R, self.reg.get_pc())
        self.reg.set_pc(jump_address, "JSR")
        return True, 'JSR', f'    {oct(self.reg.get_pc())} {oct(instruction)} JSR R{R} DD:{oct(DD)} address:{oct(jump_address)}'

    def MARK(self, instruction):
        """00 64 NN mark 46-1"""
        # *** unimplemented
        return True, 'MARK', f'{oct(instruction)} unimplemented'

    def MFPI(self, instruction):
        """00 65 SS move from previous instruction space 4-77"""
        # *** unimplemented
        return True, 'MFPI', f'{oct(instruction)} unimplemented'

    def MTPI(self, instruction):
        """00 66 DD move to previous instruction space 4-78"""
        # *** unimplemented
        return True, 'MTPI', f'{oct(instruction)} unimplemented'

    def is_other_op(self, instruction):
        """Using instruction bit pattern, determine whether it's a no-operand instruction"""
        masked1 = instruction & 0o777700
        masked2 = instruction & 0o777000
        return masked1 in [0o000200, 0o002000, 0o004000, 0o006400, 0o006500, 0o006600] or masked2 in [0o004000]

    def do_other_op(self, instruction):
        """dispatch a leftover opcode"""
        # parameter: opcode of form that doesn't fit the rest
        result = False
        self.sw.start("other")
        try:
            masked1 = instruction & 0o777700
            masked2 = instruction & 0o777000
            if masked2 in [0o004000]:
                opcode = masked2
            else:
                opcode = masked1
            result, assembly, report = self.other_instructions[opcode](instruction)
        except KeyError:
            assembly = f'{oct(instruction)}'
            report = 'Error: other opcode not found'
            result =  False
        self.sw.stop("other")
        return result, '', '', assembly, report
