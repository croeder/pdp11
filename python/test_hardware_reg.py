# test_hardware_ram    .py
# test the pdp11_hardware.py module using pytest
# pip3 install --upgrade pip
# pip install -U pytest

import logging
import threading

from pdp11_hardware import Ram
from pdp11_hardware import Registers as reg
from pdp11_hardware import PSW
from pdp11_hardware import Stack
from pdp11_hardware import AddressModes as am

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

    def test_registers(self):
        for i in range(0,7):
            test_value = 0o123223 + i
            self.reg.set(i, test_value)
            assert self.reg.get(i) == test_value

    def test_pc(self):
        test_value = 0o165250
        expected_value = test_value
        expected_value_2 = test_value + 2
        self.reg.set_pc(test_value, "test_pc")
        assert self.reg.get_pc() == expected_value
        self.reg.inc_pc("test_pc")
        assert self.reg.get_pc() == expected_value_2
        self.reg.set_pc(test_value, "test_pc")
        self.reg.inc_pc("test_pc")
        assert self.reg.get_pc() == expected_value_2

    def test_set_pc_2x_offset(self):
        test_value = 0o165250
        test_offset = 0o123
        expected_value = test_value + 2 + (test_offset) * 2
        self.reg.set_pc_2x_offset(test_offset, "test_pc")
        assert self.reg.get_pc() == expected_value

        test_offset = 0o277
        self.reg.set_pc_2x_offset(test_offset, "test_pc")
        expected_value = 0o165316
        assert self.reg.get_pc() == expected_value

    def test_sp(self):
        test_value = 0o123221
        self.reg.set_sp(test_value)
        assert self.reg.get_sp() == test_value

