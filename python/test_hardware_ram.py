# test_hardware_ram.py
# test the pdp11_hardware.py module using pytest
# pip3 install --upgrade pip
# pip install -U pytest

import logging
import threading

from pdp11_logger import Logger
from pdp11_config import Config
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
    # assert 'actual' == 'expected'
    reg = reg()
    config = Config()
    Logger()
    ram = Ram(threading.Lock(), reg, 16)

    def test_ram_16(self):
        logging.info('test_ram_16')
        ram = Ram(threading.Lock(), reg, bits=16)
        assert ram.top_of_memory == 0o177777
        assert ram.io_space == 0o160000

    def test_ram_18(self):
        logging.info('test_ram_18')
        ram = Ram(threading.Lock(), reg, bits=18)
        assert ram.top_of_memory == 0o777777
        assert ram.io_space == 0o760000

    def test_ram_24(self):
        logging.info('test_ram_24')
        ram = Ram(threading.Lock(), reg, bits=24)
        assert ram.top_of_memory == 0o77777777
        assert ram.io_space == 0o77760000

    def test_ram_byte_1(self):
        logging.info('test_ram_byte_1')
        test_value = 0o107
        test_address = 0o4123
        self.ram.write_byte(test_address,test_value)
        assert self.ram.memory[test_address] == test_value
        assert self.ram.read_byte(test_address) == test_value

    def test_ram_byte_2(self):
        logging.info('test_ram_byte_2')
        test_value = 0o270
        test_address = 0o4123
        self.ram.write_byte(test_address,test_value)
        assert self.ram.memory[test_address] == test_value
        assert self.ram.read_byte(test_address) == test_value

    def test_ram_bytes(self):
        logging.info('test_ram_bytes')
        test_value = 0o105
        test_address = self.ram.top_of_memory - 1
        self.ram.write_byte(test_address,test_value)
        assert self.ram.read_byte(test_address) == test_value

        for i in range (0,0o277,1):
            mem_test_value =  i
            self.ram.write_byte(i, mem_test_value)

        for i in range (0,0o277,2):
            mem_test_value =  i
            assert self.ram.read_byte(i) == mem_test_value


    def test_ram_word_1(self):
        logging.info('test_ram_word_1')
        test_value = 0o107070
        test_address = 0o6234

        self.ram.write_word(test_address,test_value)

        actual_value = self.ram.read_word(test_address)
        assert self.ram.memory[test_address] == test_value & MASK_LOW_BYTE
        assert self.ram.memory[test_address + 1] == (test_value & MASK_HIGH_BYTE) >> 8
        assert  actual_value == test_value

    def test_ram_word_2(self):
        logging.info('test_ram_word_2')
        test_value = 0o170707
        test_address = 0o6234

        self.ram.write_word(test_address,test_value)

        actual_value = self.ram.read_word(test_address)
        assert self.ram.memory[test_address] == test_value & MASK_LOW_BYTE
        assert self.ram.memory[test_address + 1] == (test_value & MASK_HIGH_BYTE) >> 8
        assert  actual_value == test_value

    def test_ram_word_top(self):
        logging.info('test_ram_word_top')
        test_value = 0o123232
        test_address = self.ram.top_of_memory - 1

        self.ram.write_word(test_address,test_value)

        assert self.ram.memory[test_address] == test_value & MASK_LOW_BYTE
        assert self.ram.memory[test_address + 1] == (test_value & MASK_HIGH_BYTE) >> 8
        assert self.ram.read_word(test_address) == test_value

    def test_ram_words(self):
        logging.info('test_ram_words')
        test_value = 0o123234
        for i in range (1000,2000,2):
            mem_test_value = test_value + i
            self.ram.write_word(i, mem_test_value)

        for i in range (1000,2000,2):
            expected_value = test_value + i
            actual_value = self.ram.read_word(i)
            assert actual_value == expected_value

    def test_ram_wordbyte(self):
        logging.info('test_ram_wordbyte')
        test_base = 0o10000
        test_range = 0o30000
        for value in range(0, test_range, 2):
            test_address = test_base + value
            self.ram.write_word(test_address, value)

        for value in range(0, test_range, 2):
            test_address_low = test_base + value
            test_address_high = test_base + value +1

            low_byte = self.ram.read_byte(test_address_low)
            high_byte = self.ram.read_byte(test_address_high)
            sum = low_byte + (high_byte << 8)

            expected_low_byte = value & MASK_LOW_BYTE
            expected_high_byte = (value & MASK_HIGH_BYTE) >> 8

            assert low_byte == expected_low_byte
            assert high_byte == expected_high_byte
            assert sum == value