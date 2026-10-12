# test_hardware_psw.py
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


def add_byte(psw, b1, b2):
    """add byte, limit to 8 bits, set PSW"""
    result = b1 + b2
    if result > (result & MASK_LOW_BYTE):
        psw.set_psw(v=1)
    if result == 0:
        psw.set_psw(z=1)
    result = result & MASK_LOW_BYTE
    return result


def subtract_byte(psw, b1, b2):
    """subtract bytes b1 - b2, limit to 8 bits, set PSW"""
    result = b1 - b2
    if result < 0:
        psw.set_psw(n=1)
    result = result & MASK_LOW_BYTE
    return result


def add_word(psw, b1, b2):
    """add words, limit to 16 bits, set PSW"""
    result = b1 + b2
    if result > (result & MASK_WORD):
        psw.set_psw(v=1)
    if result == 0:
        psw.set_psw(z=1)
    result = result & MASK_WORD
    return result


def subtract_word(psw, b1, b2):
    """subtract words b1 - b2, limit to 16 bits, set PSW"""
    result = b1 - b2
    if result < 0:
        psw.set_psw(n=1)
    result = result & MASK_WORD
    return result

class TestClass():
    reg = reg()
    ram = Ram(threading.Lock(), reg, 16)
    psw = PSW(ram)
    stack = Stack(reg, ram, psw)
    am = am(reg, ram, psw)

    def test_psw_address(self):
        assert self.psw.psw_address == self.ram.top_of_memory - 1

    def test_psw_set(self):
        test_value = 0o123236
        self.psw.set_psw(psw=test_value)
        assert self.psw.psw == test_value

    def test_psw_mode(self):
        # mode, priority, trap, get_n, get_z, get_v, get_c, PSW
        self.psw.set_psw(mode=0)
        self.psw.set_psw(mode=3)
        actual_psw = self.psw.psw & 0o140000
        expected_PSW = 0o140000
        assert actual_psw == expected_PSW

    def test_psw_priority(self):
        self.psw.set_psw(priority=0)
        self.psw.set_psw(priority=15)
        actual_psw = self.psw.psw & 0o000340
        expected_PSW = 0o000340
        assert actual_psw == expected_PSW

    def test_psw_trap(self):
        self.psw.set_psw(trap=0)
        self.psw.set_psw(trap=1)
        actual_psw = self.psw.psw & 0o000020
        expected_PSW = 0o000020
        assert actual_psw == expected_PSW

    def test_psw_N(self):
        self.psw.set_psw(n=0)
        self.psw.set_psw(n=1)
        actual_psw = self.psw.psw & 0o000010
        expected_PSW = 0o000010
        assert actual_psw == expected_PSW

    def test_psw_Z(self):
        self.psw.set_psw(z=0)
        self.psw.set_psw(z=1)
        actual_psw = self.psw.psw & 0o000004
        expected_PSW = 0o000004
        assert actual_psw == expected_PSW

    def test_psw_V(self):
        self.psw.set_psw(v=0)
        self.psw.set_psw(v=1)
        actual_psw = self.psw.psw & 0o000002
        expected_PSW = 0o000002
        assert actual_psw == expected_PSW

    def test_psw_C(self):
        self.psw.set_psw(c=0)
        self.psw.set_psw(c=1)
        actual_psw = self.psw.psw & 0o000001
        expected_PSW = 0o000001
        assert actual_psw == expected_PSW

    def test_psw_addb(self):
        self.psw.set_psw(psw=0)
        a = 3
        b = 4
        sum = add_byte(self.psw, a, b)
        assert sum == a + b
        assert self.psw.get_v() == 0
        assert self.psw.get_z() == 0

    def test_psw_addb_v(self):
        self.psw.set_psw(psw=0)
        a = 191
        b = 191
        sum = add_byte(self.psw, a, b)
        assert sum == 126
        assert self.psw.get_v() == 1
        assert self.psw.get_z() == 0



    #def test_psw_addb_z(self):

    #def test_psw_addw(self):
    #def test_psw_addw_v(self):
    #def test_psw_addw_z(self):

    #def test_psw_subbd(self):
    #def test_psw_subbd_n(self):

    #def tesT_psw_subw(self):
    #def tesT_psw_subw_n(self):