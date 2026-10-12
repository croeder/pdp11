"""test_stack"""
import logging
import threading

from pdp11_hardware import Registers as reg
from pdp11_hardware import Ram
from pdp11_hardware import PSW
from pdp11_hardware import Stack
from pdp11_hardware import AddressModes as am

from pdp11_br_ops import br_ops
from pdp11_cc_ops import cc_ops
from pdp11_noopr_ops import noopr_ops
from pdp11_other_ops import other_ops
from pdp11_rss_ops import rss_ops
from pdp11_ss_ops import ss_ops
from pdp11_ssdd_ops import ssdd_ops

from stopwatches import StopWatches as sw

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

    br_ops = br_ops(reg, ram, psw, sw)
    cc_ops = cc_ops(psw, sw)
    noopr_ops = noopr_ops(reg, ram, psw, stack, sw)
    other_ops = other_ops(reg, ram, psw, am, sw)
    rss_ops = rss_ops(reg, ram, psw, am, sw)
    ss_ops = ss_ops(reg, ram, psw, am, sw)
    ssdd_ops = ssdd_ops(reg, ram, psw, am, sw)

    def test_jsr_R5(self):
        # pdp11-40 4-58
        logging.info ('test_jsr_R5')

        R7 = 7
        R6 = 6
        R5 = 5
        mode1 = 1
        mode2 = 2
        mode3 = 3
        mode6 = 6

        # from pdp11-40 4-59
        # arbitrary values for test:
        PC = 0o01000
        SP = 0o02000
        SBR = 0o03000
        Data0 = 0o01234
        Data1 = 0o12345

        self.reg.set(R7, PC)
        self.reg.set(R6, SP)
        self.ram.write_word(SP, Data0)
        self.reg.set(R5, Data1)
        self.ram.write_word(PC+2, SBR)

        opcode = 0o04000 | (R5 << 6) | (mode1 << 3) | R7
        logging.info (f'opcode:{oct(opcode)}')
        self.ram.write_word(PC, opcode)

        assert self.other_ops.is_other_op(opcode)
        logging.info ('is other opcode')

        reg7 = self.reg.get(R7)
        reg6 = self.reg.get(R6)
        reg5 = self.reg.get(R5)
        top = self.ram.read_word(reg6)
        self.reg.set(R7, PC+2)
        logging.info(f'before R7:{oct(reg7)} R6:{oct(reg6)} R5:{oct(reg5)} top:{oct(top)} ')

        self.other_ops.do_other_op(opcode)

        reg7 = self.reg.get(R7)
        reg6 = self.reg.get(R6)
        reg5 = self.reg.get(R5)
        top = self.ram.read_word(reg6)
        logging.info(f'after R7:{oct(reg7)} R6:{oct(reg6)} R5:{oct(reg5)} top:{oct(top)} ')

        #assert actual == expected
        assert reg7 == PC + 2
        assert reg6 == SP - 2
        assert reg5 == PC + 2
        assert top == Data1

    def test_rts(self):
        logging.info ('test_rts')

        R7 = 7
        R6 = 6
        R5 = 5
        mode1 = 1
        mode2 = 2
        mode3 = 3
        mode6 = 6

        # from pdp11-40 4-59
        # arbitrary values for test:
        PC = 0o01002
        SP = 0o02000 - 2
        SBR = 0o03000
        Data0 = 0o01234
        Data1 = 0o12345

        self.reg.set(R7, SBR)
        self.reg.set(R6, SP)
        self.ram.write_word(SP, Data1)
        self.ram.write_word(SP+2, Data0)

        opcode = 0o000200 | R5
        logging.info (f'opcode:{oct(opcode)}')
        self.ram.write_word(PC, opcode)

        assert self.other_ops.is_other_op(opcode)
        logging.info ('is other opcode')

        reg7 = self.reg.get(R7)
        reg6 = self.reg.get(R6)
        reg5 = self.reg.get(R5)
        top = self.ram.read_word(reg6)
        self.reg.set(R7, PC+2)
        logging.info(f'before R7:{oct(reg7)} R6:{oct(reg6)} R5:{oct(reg5)} top:{oct(top)} ')

        self.other_ops.do_other_op(opcode)

        reg7 = self.reg.get(R7)
        reg6 = self.reg.get(R6)
        reg5 = self.reg.get(R5)
        top = self.ram.read_word(reg6)
        logging.info(f'after R7:{oct(reg7)} R6:{oct(reg6)} R5:{oct(reg5)} top:{oct(top)} ')

        #assert actual == expected
        assert reg7 == PC
        assert reg6 == SP + 2
        assert reg5 == Data1
        assert top == Data0



    def test_mark(self):
        logging.info ('test_mark')

    def test_mtps(self):
        logging.info ('test_mtps')