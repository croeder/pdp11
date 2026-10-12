"""test JMP"""

# pdp11-40.pdf page 4-56
# PC<-(dst)
# bits 15-6 are 0o0001
# bits 5-0 are mmm rrr: mode, register
# Full flexibility of addressing modes.
# With the exception of mode 0.
# Jmp with mode 0 causes "illegal instruction" condition.
# Instrucitons are word data, therefore on even addresses.
# Fetch instruction from odd address causes"boundary error" condition.

import logging
import threading

from pdp11_hardware import Registers as reg
from pdp11_hardware import Ram
from pdp11_hardware import PSW
from pdp11_hardware import Stack
from pdp11_hardware import AddressModes as am

from pdp11_ss_ops import ss_ops

from stopwatches import StopWatches as sw
#from pdp11_boot import pdp11Boot
#from pdp11 import PDP11
#from pdp11 import pdp11Run

MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

class TestClass():
    reg = reg()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    stack = Stack(reg, ram, psw)
    am = am(reg, ram, psw)
    sw = sw()

    ss_ops = ss_ops(reg, ram, psw, am, sw)

    R0 = 0
    R1 = 1
    R2 = 2
    R3 = 3
    R4 = 4
    R5 = 5
    R6 = 6
    R7 = 7

    mode0 = 0
    mode1 = 1
    mode2 = 2
    mode3 = 3
    mode4 = 4
    mode5 = 5
    mode6 = 6
    mode7 = 7

    opcode = 0o000100

    def DD(self, mode, register):
        return (mode << 3 | register)

    def JMP(self, modeD=0, regD=0):
        return 0o000100 | self.DD(modeD, regD)

    # https://en.wikipedia.org/wiki/PDP-11_architecture#Program_counter_addressing_modes

    def test_jmp_mode_0(self):
        logging.info('test_jmp_mode_0 register - illegal')
        pc = 0o1020
        instruction = self.JMP(self.mode0, self.R0)

        self.reg.set_pc(pc, "test_jmp_mode_1")
        logging.info(f'test_jmp_mode_0 instruction:{oct(instruction)} pc:{oct(pc)}')
        assert self.ss_ops.is_ss_op(instruction)

        run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)
        # FIX: JMP to a register is illegal: the CPU stops without jumping
        # (the Python jumped to 0; real hardware traps, which is not emulated)
        PC = self.reg.get_pc()
        logging.info (f'test_jmp_mode_0 PC:{oct(PC)}')
        assert not run
        assert PC == pc

    def test_jmp_mode_1(self):
        # Register Indirect, jumps to wherever the register points - JMP (R1)
        logging.info('test_jmp_mode_1 immediate')
        pc = 0o1000
        target = 0o2000
        instruction = self.JMP(self.mode1, self.R1)

        self.reg.set_pc(pc, "test_jmp_mode_1")
        self.reg.set (self.R1, target)
        self.ram.write_word(pc, instruction)

        logging.info(f'test_jmp_mode_2 instruction:{oct(instruction)} target:{oct(target)} pc:{oct(pc)}')
        assert self.ss_ops.is_ss_op(instruction)

        self.ss_ops.do_ss_op(instruction)

        PC = self.reg.get_pc()
        logging.info(f'test_jmp_mode_1 PC:{oct(PC)}')
        assert PC == target



    def test_jmp_mode_2_pc(self):
        logging.info('test_jmp_mode_2 immediate')
        # mode j2: JMP immediate: R contains jump_address, then incremented.
        # (Not useful for JMP/JSR) - ADC #label
        pc = 0o1000
        target = 0o2000
        instruction = self.JMP(self.mode2, self.R7)

        self.reg.set_pc(pc, "test_jmp_mode_2")
        self.reg.set (self.R7, target)
        self.ram.write_word(pc, instruction)

        logging.info(f'test_jmp_mode_2 instruction:{oct(instruction)} target:{oct(target)} pc:{oct(pc)}')
        assert self.ss_ops.is_ss_op(instruction)

        self.ss_ops.do_ss_op(instruction)

        PC = self.reg.get_pc()
        logging.info(f'test_jmp_mode_2 PC:{oct(PC)}  target:{oct(target)}')
        assert PC == target

    def test_jmp_mode_2_r(self):
        logging.info('test_jmp_mode_2 immediate')
        # mode j2: JMP immediate: R contains jump_address, then incremented.
        # (Not useful for JMP/JSR) - ADC #label
        pc = 0o1000
        target = 0o2000
        instruction = self.JMP(self.mode2, self.R1)

        self.reg.set_pc(pc, "test_jmp_mode_2")
        self.reg.set (self.R1, target)
        self.ram.write_word(pc, instruction)

        logging.info(f'test_jmp_mode_2 instruction:{oct(instruction)} target:{oct(target)} pc:{oct(pc)}')
        assert self.ss_ops.is_ss_op(instruction)

        self.ss_ops.do_ss_op(instruction)

        PC = self.reg.get_pc()
        logging.info(f'test_jmp_mode_2 PC:{oct(PC)}  target:{oct(target)}')
        assert PC == target
        assert self.reg.get(self.R1) == target + 2


    # This test is based on the write-up in the processor manual
    def test_jmp_mode_3(self):
        logging.info('test_jmp_mode_3 ')
        # Mode 3, Autoincrement Indirect,
        # jumps to address contained in a word
        # addressed by the register
        # and increments the register by two - JMP @(R1)+
        # An absolute jump: JMP @#FOO results in mode 3, register 7,
        # and the second word contains the absolute address of FOO.
        pc = 0o1000
        target = 0o2000
        instruction = self.JMP(self.mode3, self.R7)

        self.ram.write_word(pc, instruction)
        self.ram.write_word(pc+2, target)
        self.reg.set_pc(pc+2, "test_jmp_mode_3")

        logging.info(f'test_jmp_mode_3 instruction:{oct(instruction)} target:{oct(target)} pc:{oct(pc)}')
        assert self.ss_ops.is_ss_op(instruction)

        self.ss_ops.do_ss_op(instruction)

        PC = self.reg.get_pc()
        logging.info(f'test_jmp_mode_3 PC:{oct(PC)}  target:{oct(target)}')
        assert PC == target


    def test_jump_mode_6(self):
        logging.info('test_jump_mode_6 Relative')
        # Mode 6, Indexed, jumps to the result of
        # adding a 16 bit word to the register specified - JMP 20(PC)
        pc = 0o001000
        target = 0o001200
        relative_address = target - pc -4 # distance between instruction and the target, less instruciton and data
        instruction = self.JMP(self.mode6, self.R7)

        self.ram.write_word(pc, instruction)
        self.ram.write_word(pc+2, relative_address)
        self.reg.set_pc(pc+2, "test_jump_mode_6")

        logging.info(f'test_jump_mode_6 instruction:{oct(instruction)} relative_address:{oct(relative_address)}')
        assert self.ss_ops.is_ss_op(instruction)

        self.ss_ops.do_ss_op(instruction)

        assert self.reg.get_pc() == target

    def test_jump_mode_7(self):
        logging.info('test_jump_mode_7 Relative Deferred')
        # Mode 7, Index Indirect, jumps to address contained in a word
        # addressed by adding a 16 bit word to the register specified - JMP @20(PC)
        instruction = 0o005077
        assert self.ss_ops.is_ss_op(instruction)
        a = 0o000020
        pc = 0o1020
        pointer = 0o1044
        address = 0o1060
        self.ram.write_word(pc, instruction)
        self.ram.write_word(pc + 2, a)
        self.ram.write_word(pointer, address)
        self.ram.write_word(address, 0o100001)
        self.reg.set_pc(pc, "test_PC_mode_7")
        self.reg.set_pc(pc + 2, "test_PC_mode_7") # why?
        self.ss_ops.do_ss_op(instruction)
        assert self.reg.get_pc() == 0o1024
        assert self.ram.read_word(address) == 0o0
        condition_codes = self.psw.get_nzvc()
        assert condition_codes == "0100"

    def not_test_m9301(self):
        # This test requires the PDP11 system
        # which depends on pySinpleGui
        # which is not available on GtHub automatic testing.
        # 165066 012701		MOV	#JMPT, R1
        # 165070 165074
        # 165072 000111		JMP	@R1
        # 165074 000121	JMPT:	JMP	(R1)+		; See M9312 Maint Man
        # 165076 012701		MOV	#JMPD, R1
        # 165100 165106
        # 165102 000131		JMP	@(R1)+
        # 165104 000111		JMP	@R1		; Goes to BR below
        # 165106 165104 JMPD:     ; jump data
        # 165110 000000     HALT
        pdp11 = PDP11()
        boot = pdp11Boot(pdp11.reg, pdp11.ram)
        pdp11run = pdp11Run(pdp11)
        jump_test = [0o012701, 0o165074, 0o000111, 0o000121,
                     0o012701, 0o165106, 0o000131, 0o000111,
                     0o165104, 0o000000]
        boot.load_machine_code(jump_test, 0o165066)
        pdp11run.run()
        assert pdp11.reg.get_pc() == 0o165112