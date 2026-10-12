"""PDP11 Registers, RAM, PSW, Stack

Fixed to follow the DEC PDP-11 processor handbook, as in the C version in
../c; each FIX comment says what the original Python did."""

import sys
import logging
import threading
import pdp11_util as u

# masks for accessing words and bytes
MASK_WORD = 0o177777
MASK_WORD_MSB = 0o100000
MASK_BYTE_MSB = 0o000200
MASK_LOW_BYTE = 0o000377
MASK_HIGH_BYTE = 0o177400

def fix_sign(word):
    """fix a negative word so it will work with python math"""
    if word & MASK_WORD_MSB:
        more_bits = sys.maxsize & ~MASK_WORD
        field = sys.maxsize - (word & MASK_WORD | more_bits) + 1
        result = -field
    else:
        result = word
    return result

def address_offset(address, offset):
    """address plus sign-fixed offset, wrapped to 16 bits.
    FIX: the Python let this go negative or past 0o177777."""
    result = (address + fix_sign(offset)) & MASK_WORD
    return result

def formatted_offset(offset):
    '''Format affset for disassembly'''
    formatted = offset
    # if the high bit is set, it's negative
    if offset & MASK_WORD_MSB == MASK_WORD_MSB:
        # mask it off and subtract what's left
        formatted = -(0o077777 - (offset & 0o077777)) * 2
    return f'{oct(formatted)}'

class Registers:
    """PDP11 registers including PC and SP"""
    def __init__(self):
        logging.info('initializing pdp11_hardware registers')
        self.registermask = 0o07
        self.__sp = 0o06  # R6 is the stack pointer
        self.__pc = 0o07  # R7 is the program counter
        self.__registers = [0, 0, 0, 0, 0, 0, 0, 0]  # R0 R1 R2 R3 R4 R5 R6 R7

    def get(self, register):
        """read a single register

        :param register: integer 0-7
        :return: register contents"""
        result = self.__registers[register]
        #logging.debug(f'; get R{register} = {oct(result)}')
        return result

    def set(self, register, value):
        """set a single register
        :param register: integer 0-7
        :param value: integer
        """
        # FIX: registers are 16 bits.  The Python asserted value <= 0o177777,
        # so it accepted negative values and crashed on carries.
        self.__registers[register] = value & MASK_WORD
        #logging.debug(f'; set R{register} = {oct(self.__registers[register])}')

    def get_pc(self):
        """get program counter without incrementing.
        :return: program counter"""
        result = self.__registers[self.__pc]
        #logging.debug(f'get_pc returns {oct(result)}')
        return result

    def inc_pc(self, whocalled=''):
        """increment progran counter by 2
        The Program Counter points to the word after the instruction being interpreted.

        :return: new program counter"""
        # FIX: wrap at 16 bits
        self.__registers[self.__pc] = (self.__registers[self.__pc] + 2) & MASK_WORD
        #logging.debug(f'inc_pc R7<-{oct(self.__registers[self.__pc])} called by {whocalled}')
        return self.__registers[self.__pc]

    def set_pc(self, newpc, whocalled=''):
        """set program counter to arbitrary value"""
        #logging.debug(f"set_pc({oct(newpc)}, {whocalled})")
        # FIX: wrap at 16 bits (the Python asserted newpc <= 0o177777)
        self.__registers[self.__pc] = newpc & MASK_WORD
        #logging.debug(f'set_pc R7<-{oct(newpc)} called by {whocalled}')

    def set_pc_2x_offset(self, offset=0, whocalled=''):
        """set program counter to 2x offset byte;
        adjust for sign bit and PC.
        Bit 7 of the offset is the sign.

        0o377 < offset < 0o177 """
        #
        # If it is set then the offset is negative.
        offset = 2 * offset
        if offset > MASK_LOW_BYTE:  # if we overflowed with the sign bit
            offset = fix_sign(offset | MASK_HIGH_BYTE)
        # FIX: wrap at 16 bits
        self.__registers[self.__pc] = (self.__registers[self.__pc] + offset) & MASK_WORD
        #logging.debug(f'set_pc_2x_offset pc={oct(self.__registers[self.__pc])} called by {whocalled} ')

    def get_sp(self):
        """get stack pointer

        :return: stack pointer"""
        return self.__registers[self.__sp]

    def set_sp(self, newsp, whocalled=''):
        """set stack pointer

        :return: new stack pointer"""
        # FIX: wrap at 16 bits (the Python asserted newsp <= 0o177777)
        self.__registers[self.__sp] = newsp & MASK_WORD

    def registers_to_string(self):
        """logging.info all the registers in the log"""
        index = 0
        report = ''
        for register in self.__registers:
            report = f'{report} {u.oct6(register)}'  # f'R{index}:{oct(register)}  '
            index = index + 1
        return report

class Ram:
    """PDP11 Random Access Memory including I/O page"""
    # Sixteen-bit words are stored little-endian.
    # Bytes can be on even or odd addresses
    # Words must be on even addresses,

    # Odd-numbered bytes are high.
    # Even-numbered bytes are low.
    # for example
    # 0o271 - high byte
    # 0o270 - low byte

    def __init__(self, lock, reg, bits=16):
        logging.info(f'initializing pdp11_hardware.Ram({bits})')
        self.reg = reg

        # set up basic memory boundaries
        # 16 bits   0o177777 = 0x0000FFFF 64kB 32kw
        # 18 bits   0o777777 = 0x0003FFFF 256kB 128kw
        # 24 bits 0o17777777 = 0x003FFFFF 4MB 4096kB 2048kw

        bits = int(bits)
        self.top_of_memory = 2**bits-1

        # the io page is the top 4k words (8kB) of memory
        self.io_space = self.top_of_memory - 0o017777
        logging.info(f'{bits} bits -> top_of_memory: {oct(self.top_of_memory)} = {self.top_of_memory}; io_space:{oct(self.io_space)} io_space={self.io_space}')

        # instantiate the byte array
        # Nobody outside this class is supposed to access this.
        # Sometimes I really hate Python and people's attitudes when you want a feature. 
        # For example, when people suggest that Python shoudl have private variables, 
        # the responses are the usual: you don't need that, don't write it that way, whaty's wrong with ugly code? 
        self.memory = bytearray(self.top_of_memory+1)

        # set up always-ready i/o device status words
        #self.write_word(self. TKS, 0o000000)
        #self.write_word(self. TPS, 0b0000000011000000)  # always xmit ready and interrupt enabled

        # psw class handles updating PSW in ram.
        # Only a pdp11 program should read this from ram.

        # io map is dictionaries of addresses, methods, and locks
        self.iomap_readers = {}
        self.iomap_writers = {}

        # set up the vector space
        # the bottom area is io device handler vectors
        self.top_of_vector_space = 0o274
        for address in range(0o0, self.top_of_vector_space):
           self.write_byte(address, 0o277)

        # Initialize the shadow RAM for io page.
        # If there's a read or write to some address in io page
        # that's not been assigned, it will just read 0 or whatever was written.
        for address in range(self.io_space, self.top_of_memory):
           self.write_byte(address, 0o0)

        self.lock = lock
        self.i_set_lock = False
        logging.info(f'Ram init done. Lock object:{self.lock}')

    # emulator interface to locking
    # The idea is that any time there's unlocked access to IO page RAM,
    # It's the CPU doing it and that section needs to be locked.
    def get_lock(self, whocalled=''):
        if not self.lock.locked(): #self.ram.lock.is_set(): # lock was NOT set
            self.lock.acquire()
            logging.info(f'{whocalled} got lock')
            self.i_set_lock = True

    def release_lock(self, whocalled=''):
        if self.i_set_lock:
            self.lock.release()
            logging.debug(f'{whocalled} released lock')
            self.i_set_lock = False

    def safe_character(self, byte):
        """return character if it is printable"""
        if byte <= 31:
            low_ascii = ['NUL', 'SOH', 'STX', 'ETX', 'EOT', 'ENQ', 'ACK', 'BEL',
                         'BS', 'HT', 'LF', 'VF', 'FF', 'CR', 'SO', 'SI',
                         'DLE', 'DC1', 'DC2', 'DC3', 'DC4', 'NAK', 'SYN', 'ETB',
                         'CAN', 'EM', 'SUB', 'ESC', 'FS', 'GS', 'RS', 'US']
            result = low_ascii[byte]
        elif byte <= 127:
            result = chr(byte)
        else:
            result = ''
        return result

    def register_io_writer(self, device_address, method):
        """map i/o write handler into memory"""
        assert device_address < self.top_of_memory
        assert device_address >= self.io_space
        # The actual criteria are a little more stringent.
        logging.info(f'register_io_writer({oct(device_address)}, {method.__name__})')
        self.iomap_writers[device_address] = method

    def register_io_reader(self, device_address, method):
        """map i/o read handler into memory"""
        assert device_address < self.top_of_memory
        assert device_address >= self.io_space
        # The actual criteria are a little more stringent.
        logging.info(f'register_io_reader({oct(device_address)}, {method.__name__})')
        self.iomap_readers[device_address] = method
        # This has been confirmed to work from here

    def write_byte(self, address, data):
        """write a byte to memory.
        Address can be even or odd"""
        assert address <= self.top_of_memory    # *** should be a trap
        data = data & MASK_LOW_BYTE
        if address in self.iomap_writers:
            if data != 0:
                logging.debug(f'write_byte io self_lock:{self.lock.locked()} @{oct(address)}, {oct(data)} {self.safe_character(data)}')
            self.get_lock()
            self.iomap_writers[address](data)
            self.release_lock()
        else:
            #logging.debug(f'; write_byte({u.oct6(address)}, {u.oct3(data)} {self.safe_character(data)})')
            self.memory[address] = data

    def read_byte(self, address):
        """Read one byte of memory.
        Address can be even or odd."""
        assert address <= self.top_of_memory
        if address in self.iomap_readers:
            #logging.debug(f'read_byte IO(@{oct(address)})')
            self.get_lock()
            result = self.iomap_readers[address]()
            self.release_lock()
            if result != 0:
                logging.debug(f'read_byte io self_lock:{self.lock.locked()} @{oct(address)} returns {oct(result)} {self.safe_character(result)}')
        else:
            result = self.memory[address]
            #logging.debug(f'; read byte {u.oct6(address)}) = {u.oct3(result)}')
        return result

    def write_word(self, address, data):
        """write a two-word data chunk to memory.
        Address must be even.

        :param address:
        :param data:
        """
        #logging.debug(f'write_word(@{oct(address)}, {oct(data)})')
        # FIX: the PDP-11/03 (LSI-11) ignores bit 0 of a word address rather
        # than trapping; the Python asserted the address was even.
        address &= ~1
        assert address < self.top_of_memory   # *** should be a trap
        assert data <= MASK_WORD
        if address in self.iomap_writers:
            logging.debug(f'write_word io @{oct(address)}, {oct(data)}')
            self.get_lock()
            self.iomap_writers[address](data)
            self.release_lock()
        else:
            self.memory[address + 1] = (data & MASK_HIGH_BYTE) >> 8
            self.memory[address] = data & MASK_LOW_BYTE
            #logging.debug(f'write_word RAM(@{oct(address)}, {oct(data)})')

    def read_word(self, address):
        """Read a word of memory.
        Address must be even.
        Returns a two-byte value.
        Crashes simulator if we go out of bounds."""
        # Low bytes are stored at even-numbered memory locations.
        # High bytes at are stored at odd-numbered memory locations.
        address &= ~1   # FIX: the 11/03 ignores bit 0 of a word address
        assert address < self.top_of_memory   # *** should be a trap
        result = ""
        if address in self.iomap_readers:
            #logging.debug(f'read word IO({u.oct6(address)})')
            self.get_lock()
            result =  self.iomap_readers[address]()
            self.release_lock()
            logging.debug(f'read word IO({u.oct6(address)}) returns {u.oct6(result)}')
        else:
            result = (self.memory[address + 1] << 8) + self.memory[address]
            #logging.debug(f'read word RAM {u.oct6(address)} = {u.oct6(result)}')
        return result

    def read_word_from_pc(self):
        """Read word from PC and increment PC"""
        pc = self.reg.get_pc()
        result = self.read_word(pc) # this is buggy?
        #logging.debug(f'read_word_from_pc {oct(pc)} returns {oct(result)}')
        self.reg.inc_pc("read_word_from_pc")
        return result

    #    MASK_LOW_BYTE = 0o000277
    #    MASK_WORD = 0o177777
    #    MASK_BYTE_MSB = 0o000200
    #    MASK_WORD_MSB = 0o100000
    # 16 bits   0o177777 = 0x0000FFFF
    # 18 bits   0o777777 = 0x0003FFFF
    # 22 bits 0o17777777 = 0x003FFFFF

    def dump(self, start, stop):
        """logging.info out memory from a nice address before start to a nice address after stop.
        Nice means rounded off to a bunch of bytes convenient to display on a line.
        logging.info a row as octal Words followed by octal Bytes followed by ascii interpretation
        Octal word is 7 chars with space. Octal byte is 4 chars with space. acii byte is 1 char. 12 chars
        :param start:
        :param stop:
        :return:
        """
        display_bytes = 8 # display this may bytes across the line
        start = start & 0o177770
        stop = (stop & 0o177770) + 0o010
        logging.info(f'dump({oct(start)}, {oct(stop)})')
        logging.debug(f'@@@@@@ octal                        bytes                            ascii   ')

        logging.info_line = ""
        for row_address in range(start, stop, display_bytes):
            logging.info_line =   f'{u.oct6(row_address)} '
            logging.info_line = logging.info_line + " "
            for word_address in range(row_address, row_address+display_bytes, 2):
                logging.info_line = logging.info_line +  f'{u.oct6(self.read_word(word_address))} '
                #+" " self.read_word(word_address)
            logging.info_line = logging.info_line + " "
            for byte_address in range(row_address, row_address+display_bytes):
                logging.info_line = logging.info_line + f'{u.oct3(self.read_byte(byte_address))} '
                #+" " self.read_byte(byte_address)
            logging.info_line = logging.info_line + " "
            for byte_address in range(row_address, row_address+display_bytes):
                byte = self.read_byte(byte_address)
                if byte in range(16,127):
                    char = chr(byte)
                else:
                    char = "."
                logging.info_line = logging.info_line + char
            logging.info(logging.info_line)

class PSW:
    """PDP11 Processor Status Word"""
    def __init__(self, ram):
        """initialize PDP11 PSW"""
        logging.info('initializing pdp11_hardware psw')
        # 104000-104377 EMT (trap & interrupt)
        # 104400-104777 TRAP (trap & interrupt)

        # 013400 0 001 011 100 000 000 BCS (branch)
        # 16SSDD 1 110 *** *** *** *** subtract src from dst (double)
        # 06SSDD 0 110 *** *** *** *** ADD add src to dst (double)

        # PSW bit meanings and masks
        # 15 14 current mode        0o140000
        # 13 12 previous mode       0o030000
        # 7 6 5 priority            0o000340
        # 4 T trap                  0o000020
        # 3 get_n result was negative   0o000010
        # 2 get_z result was zero       0o000004
        # 1 get_v overflow              0o000002
        # 0 get_c result had carry      0o000001

        self.ram = ram
        self.psw_address = ram.top_of_memory - 1  # 0o177776

        # This class keeps the deinitive version of the PSW.
        # When it changes, this class sets it into RAM.
        self.psw = 0o0

        self.c_mode_mask = 0o140000
        self.p_mode_mask = 0o030000
        self.mode_mask = 0o170000
        self.priority_mask = 0o000340
        self.trap_mask = 0o000020

        # NZVC condition codes
        self.n_mask = 0o000010  # Negative
        self.z_mask = 0o000004  # Zero
        self.v_mask = 0o000002  # Overflow
        self.c_mask = 0o000001  # Carry

        # set up psw as an io device so we're not constantly writing to ram
        self.ram.register_io_reader(self.psw_address, self.get_psw)
        logging.info(f'psw initilialized @{oct(self.psw_address)}')

    def get_psw(self):
        return self.psw

    def set_psw(self, mode=-1, priority=-1, trap=-1, n=-1, z=-1, v=-1, c=-1, psw=-1):
        """set processor status word by optional parameter

        :param mode:
        :param priority:
        :param trap:
        :param n: negative
        :param z: zero
        :param v: overflow
        :param c: carry
        :param psw: processor status word
        :return:
        """
        #logging.debug(f'set_psw(mode={mode}, priority={priority}, trap={trap}, n={n}, z={z}, v={v}, c={c}, PSW={oct(psw)})')
        #logging.debug(f'set_psw self.psw:{self.psw}')
        if psw > -1:
            self.psw = psw
            #logging.debug(f'set_psw PSW self.psw:{self.psw}')
        if mode > -1:
            oldmode = self.psw & self.mode_mask
            self.psw = (self.psw & ~self.c_mode_mask) | (mode << 14) | (oldmode >> 2)
            #logging.debug(f'set_psw mode self.psw:{self.psw}')
        if priority > -1:
            self.psw = (self.psw & ~self.priority_mask) | (priority << 5)
            #logging.debug(f'set_psw priority self.psw:{self.psw}')
        if trap > -1:
            self.psw = (self.psw & ~self.trap_mask) | (trap << 4)
            #logging.debug(f'set_psw trap self.psw:{self.psw}')
        if n > -1:
            self.psw = (self.psw & ~self.n_mask) | (n << 3)
            #logging.debug(f'set_psw get_n self.psw:{self.psw}')
        if z > -1:
            self.psw = (self.psw & ~self.z_mask) | (z << 2)
            #logging.debug(f'set_psw get_z self.psw:{self.psw}')
        if v > -1:
            self.psw = (self.psw & ~self.v_mask) | (v << 1)
            #logging.debug(f'set_psw get_v self.psw:{self.psw}')
        if c > -1:
            self.psw = (self.psw & ~self.c_mask) | c
            #logging.debug(f'set_psw get_c self.psw:{self.psw}')

    def set_nzvc(self, value):
        """set condition codes (last four bits of psw)"""
        non_psw_bits = 0o177760
        psw_bits = 0o000017
        self.psw = (self.psw & non_psw_bits) | (value & psw_bits)

    def set_n(self, b, value):
        """set condition code get_n based on the msb negative bit

        :param b: "B" or "" for Byte or Word
        :param value: value to test
        """
        if b == 'B':
            n_mask = MASK_BYTE_MSB
        else:
            n_mask = MASK_WORD_MSB
        if (value & n_mask) == n_mask:
            n = 1
        else:
            n = 0
        self.set_psw(n=n)
        return n

    def set_z(self, b, value):
        """set condition code get_z based on the value

        :param b: "B" or "" for Byte or Word
        :param value: value to test
        """
        if b == 'B':
            z_mask = MASK_LOW_BYTE
        else:
            z_mask = MASK_WORD
        if (value & z_mask) == 0:
            z = 1
        else:
            z = 0
        self.set_psw(z=z)
        return z

    def set_v(self, b, value):
        """set condition code v based on the value
        :param b: "B" or "" for Byte or Word
        :param value: value to test
        """
        v = 0
        if b == 'B':
            if value > MASK_LOW_BYTE:
                v = 1
        elif b == 'W':
            if value > MASK_WORD:
                v = 1
        # FIX: the Python called self.set_psw(v), which passes v as mode.
        self.set_psw(v=v)
        return v

    def get_n(self):
        """negative status bit of PSW"""
        return (self.psw & self.n_mask) >> 3

    def get_z(self):
        """zero status bit of PSW"""
        return (self.psw & self.z_mask) >> 2

    def get_v(self):
        """overflow status bit of PSW"""
        return (self.psw & self.v_mask) >> 1

    def get_c(self):
        """carry status bit of PSW"""
        return self.psw & self.c_mask

    def get_nzvc(self):
        """return string containing NZVC bits"""
        return f'{self.get_n()}{self.get_z()}{self.get_v()}{self.get_c()}'

class Stack:
    """PDP11 Stack"""
    def __init__(self, reg, ram, psw):
        logging.info('initializing pdp11_hardware stack')
        self.reg = reg
        self.ram = ram
        self.psw = psw

    # ****************************************************
    # stack methods for use by instructions
    # ****************************************************
    def push(self, value):
        """push the value onto the stack

        decrement stack pointer, write value to that address
        """
        stack = (self.reg.get_sp() - 2) & MASK_WORD
        self.reg.set_sp(stack)
        self.ram.write_word(stack, value)

    def pop(self):
        """pop value off the stack

        get value from stack pointer, increment stack pointer"""
        stack = self.reg.get_sp()
        result = self.ram.read_word(stack)
        self.reg.set_sp(stack + 2)
        return result

    def trap(self, vector):
        """NEW: the trap sequence, for BPT and IOT.
        push PS, push PC, PC <- (vector), PS <- (vector+2)"""
        self.push(self.psw.psw & MASK_WORD)
        self.push(self.reg.get_pc())
        self.reg.set_pc(self.ram.read_word(vector), "trap")
        self.psw.set_psw(psw=self.ram.read_word(vector + 2))

class AddressModes:
    '''Implements the 8 standrad address modes of the PDP11 instruction set.
    Every instruction that needs to set these up calls addressing_mode_get to get the praneter,
    addressing_mode_R7 to implement program counter jumps, and
    addressing_mode_set to handle address modes for destination register.
    Autoincrement and autodecrement operations on a register are
    by 1 in byte instructions, by 2 in word instructions,
    and by 2 whenever a deferred mode is used,
    since the quantity the register addresses is a (word) pointer.'''
    def __init__(self, reg, ram, psw):
        logging.info('initializing pdp11_hardware addressModes')
        self.reg = reg
        self.ram = ram
        self.psw = psw

        self.address_modes_used = {0:0, 1:0, 2:0, 3:0, 4:0, 5:0, 6:0, 7:0}

    def resolve(self, b, mode_register, fetch):
        """Resolve an operand specifier.  With fetch False the effective
        address is computed but the operand itself is not read.  NEW: used for
        MOV, CLR and SXT destinations, which the hardware writes without
        reading, so that a write to a device register does not first trigger
        the device's read.

        :param b: "B" for byte, anything else for word
        :param mode_register: bits of opcode that contain address mode and register number
        :return: operand, register, address, operand_word, assembly, addressmode
        """
        # mode_register contains the six bits that specify the mode and the register.
        # This could be for either of the two parameters of an SSDD instruction.
        # PC points to the word after the instruction.
        addressmode = (mode_register & 0o70) >> 3

        self.address_modes_used[addressmode] = self.address_modes_used[addressmode] + 1

        register = mode_register & 0o07
        address = 0
        operand = 0
        operand_word = ''
        if addressmode != 0 and register==7:
            operand_word = u.oct6(self.ram.read_word(self.reg.get(7)))
        assembly = ''

        if b == 'B':
            ram_read = self.ram.read_byte
            increment = 1
        else:
            ram_read = self.ram.read_word
            increment = 2
        if register in (6, 7):
            increment = 2

        if addressmode == 0:
            # Register: register contains operand
            operand = self.reg.get(register)
            assembly = f'R{register}'
        elif addressmode == 1:
            # Register Deferred: register contains address of operand
            address = self.reg.get(register)
            assembly = f'@R{register}'
        elif addressmode == 2:
            # Autoincrement: register contains address of operand then incremented
            address = self.reg.get(register)
            self.reg.set(register, self.reg.get(register) + increment)
            assembly = f'(R{register})+'
        elif addressmode == 3:  # autoincrement deferred
            pointer = self.reg.get(register)
            address = self.ram.read_word(pointer)
            self.reg.set(register, self.reg.get(register) + 2)
            assembly = f'@(R{register})+'
        elif addressmode == 4:  # autodecrement direct
            self.reg.set(register, self.reg.get(register) - increment)
            address = self.reg.get(register)
            assembly = f'-(R{register})'
        elif addressmode == 5:  # autodecrement deferred
            self.reg.set(register, self.reg.get(register) - 2)
            pointer = self.reg.get(register)
            address = self.ram.read_word(pointer)
            assembly = f'@-(R{register})'
        elif addressmode == 6:  # index
            # Value X, stored in a word following the instruction,
            # is added to the register.
            # The sum contains the address of the operand.
            # Neither X nor the register are modified.
            x = self.ram.read_word_from_pc()
            address = address_offset(self.reg.get(register), x)
            operand_word = u.oct6(x)
            assembly = f'{formatted_offset(x)}(R{register})'
        elif addressmode == 7:  # index deferred
            x = self.ram.read_word_from_pc()
            pointer = address_offset(self.reg.get(register), x)
            address = self.ram.read_word(pointer)
            operand_word = u.oct6(x)
            assembly = f'@{formatted_offset(x)}(R{register})'

        if addressmode != 0 and fetch:
            operand = ram_read(address)
        return operand, register, address, operand_word, assembly, addressmode

    def addressing_mode_get(self, b, mode_register):
        """copy the value from the location indicated by mode_register

        :param b: "W" for word, "B" for byte
        :param mode_register: bits of opcode that contain address mode and register number
        :return: operand, register, address, operand_word, assembly, addressmode
        """
        return self.resolve(b, mode_register, True)

    def addressing_mode_address(self, b, mode_register):
        """NEW: like addressing_mode_get, but does not read the operand"""
        return self.resolve(b, mode_register, False)

    # https://retrocomputing.stackexchange.com/questions/9248/pdp-11-jmp-and-jsr-how-was-the-target-specified
    # Mode 0 - illegal
    # Mode 1, Register Indirect, jumps to wherever the register points - JMP (R1)
    # Mode 2 (Autoincrement) becomes immediate (Not useful for JMP/JSR) -  ADC #label
    # Mode 3 (Autoincrement Indirect) becomes Absolute - JMP @#label
    # Mode 3, Autoincrement Indirect, jumps to address contained in a word addressed by the register and increments the register by two - JMP @(R1)+ (*3)
    # Mode 6 (Indexed) is the already mentioned Relative -  JMP label
    # Mode 6, Indexed, jumps to the result of adding a 16 bit word to the register specified - JMP 20(PC)
    # Mode 7, Index Indirect, jumps to address contained in a word addressed by adding a 16 bit word to the register specified - JMP @20(PC)
    # section 5 of the MACRO-11 manual

    # PDP-11 IAS/RSX-11 MACRO-11 Reference Manual:
    # See page B-2 for mores and for JMP
    # In the autoincrement mode, both the JMP and JSR instructions autoincrement the register before its
    # use on the PDP-11/40, but not on the PDP-11/45 o r 11/10.
    #
    # In double operand instructions having the addressing form Rn, (Rn) + or Rn, -(Rn),
    # # where the source and destination registers are the same, the source operand is evaluated as the autoincremented
    # or autodecremented value, but the destination register, at the time it is used, still contains the originally-intended effective address.
    # In the following example, as executed on the PDP-11/40, Register 0 originally contains 100(8):
    # MOV 100,R0
    # MOV R0,(R0)+  ; 102 is moved to R0
    # MOV R0,-(R0)  ; 75 is moved to R0
    # avoid these forms because they don't always work the same
    # MOV #100,R0  ; assembles into two words.
    #       ;Processor fetches MOV and increments PC.
    #       ;Processor fetches 100 and increments PC.

    # Questions on MACRO-11
    # It is source,target
    # MOV #100,R0 ; move 100 into Register 0
    # can you have two autoincrement or autodecrement addresses?

    # Branch instructions

    def addressing_mode_jmp(self, mode_register):
        """JMP and JSR use the effective address of their destination as the
        jump target (modes 1-7; mode 0 is illegal).
        FIX: the Python read one word too many in modes 4 and 5, and in mode 7
        read a further word and added the PC to it."""
        # See page A-11 pf pdp11/40.pdf
        if mode_register & 0o70 == 0:
            # JMP direct illegal register address; halt
            # *** this should call a trap
            self.address_modes_used[0] = self.address_modes_used[0] + 1
            return False, 0o0, '', f'R{mode_register & 0o07}'
        _, _, jump_address, operand_word, assembly, _ = self.resolve('', mode_register, False)
        return True, jump_address, operand_word, assembly

    def addressing_mode_set(self, b, addressmode, result, register, address):
        """copy the result into the register or address specified

        Parameters:
            b: 'B' or ''
            addressmode: address mode of destination
            result: word or byte
            register: if address is zero, put it in this register
            address: otherwise put it here
        """
        #logging.debug(f'addressing_mode_set "{b}" result:{oct(result)} register:{register} address:{address}')
        if addressmode == 0:
            self.reg.set(register, result)
        else:
            if b == 'B':
                self.ram.write_byte(address, result)
            else:
                self.ram.write_word(address, result & MASK_WORD)

    def address_mode_report(self):
        """
        logging.info list of counts of address modes used during run.
        """
        logging.info('address modes used:')
        for addressmode in range(0,8):
            logging.info(f'{addressmode}:{self.address_modes_used[addressmode]}')
            