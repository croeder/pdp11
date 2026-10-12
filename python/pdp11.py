"""PDP-11 Emulator

This copy runs on a stdin/stdout terminal (pdp11_terminal.Terminal).  The
PySimpleGUI consoles (VT52, Tektronix 4010) and their run methods are not
in this folder; see the repository root for them.  FIX comments say what
the original Python did.
"""
import logging
import threading  # FIX: the Python used threading without importing it

import pdp11_util as u
from pdp11_logger import Logger
from pdp11_config import Config
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

from pdp11_dl11 import DL11
from pdp11_terminal import Terminal
from pdp11_boot import pdp11Boot as boot
from pdp11_m9301 import M9301
from stopwatches import StopWatches as sw

# boot.load_machine_code(boot.bootstrap_loader, bootaddress)
# reg.set_pc(0o2000, "load_machine_code")
# self.ram.dump(0o2000, 0o2064)

# boot.load_machine_code(boot.echo, boot.echo_address)
# self.ram.dump(0o1000, 0o1020)

# boot.read_PDP11_assembly_file('source/M9301-YF.txt')
# self.reg.set_pc(0o165022, "load_machine_code")

# source/M9301-YA.txt - includes assembly; starts at 165000
# source/M9301-YF.txt - includes assembly; starts at 165022
# source/M9301-YB.txt - raw machine, not very useful in diagnosing anything
# source/M9301-YH.txt - raw machine, not very useful in diagnosing anything
# self.ram.dump(0o165000, 0o165000+32)

# multiprocessing
# https://docs.python.org/3/library/multiprocessing.html#exchanging-objects-between-processes
# the i/o page becomes the shared object.

class PDP11():
    """Timber's PDP11 emulator"""
    def __init__(self, ui="None"):
        """instantiate the PDP11 emulator components"""
        Logger()
        logging.info(f'pdp11CPU initializing with ui={ui}')
        self.sw = sw()
        self.lock = threading.Lock()

        config = Config()

        # hardware
        self.reg = reg()
        self.ram = Ram(self.lock, self.reg, config.lookup('ram', 'bits'))
        self.psw = PSW(self.ram)
        self.stack = Stack(self.reg, self.ram, self.psw)
        self.am = am(self.reg, self.ram, self.psw)

        # operations
        self.br = br_ops(self.reg, self.ram, self.psw, self.sw)
        self.noopr_ops = noopr_ops(self.reg, self.ram, self.psw, self.stack, self.sw)
        self.ss_ops = ss_ops(self.reg, self.ram, self.psw, self.am, self.sw)
        self.ssdd_ops = ssdd_ops(self.reg, self.ram, self.psw, self.am, self.sw)
        self.rss_ops = rss_ops(self.reg, self.ram, self.psw, self.am, self.sw)
        self.other_ops = other_ops(self.reg, self.ram, self.psw, self.am, self.sw)
        self.cc_ops = cc_ops(self.psw, self.sw)

        self.executed = {}
        self.CPU_cycles = 0

        # Set up event so control of whether CPU is running doesn't get stepped on
        self.run = False
        self.runEvent = threading.Event()
        self.runEvent.set()

        # i/o devices
        self.boot = boot(self.reg, self.ram)
        self.m9301 = M9301(self.reg, self.ram, self.boot)
        #self.rk11 = RK11(self.ram)

        # set up DL11
        # set up the serial interface addresses
        # this must eventually be definable in a file so it has to be here
        # reader status register 177560
        logging.info('pdp11CPU setting up DL111 at 0o177560')
        self.dl11 = DL11(self.ram, 0o177560)
        # FIX: the Python imported the PySimpleGUI consoles unconditionally,
        # and its "if VT52 ... if TEK4010 ... else" gave a VT52 machine a
        # terminal too.  Here the ui argument is only logged.
        logging.info(f'pdp11CPU initializing ui:{ui}')
        self.terminal = Terminal(self.dl11, self.sw)

        logging.info('pdp11CPU initializing done')

    def set_run(self, new_run):
        """thread-safe run setter"""
        self.runEvent.wait()
        self.runEvent.clear()
        if self.run != new_run:
            self.run = new_run
            logging.info(f'set_run({self.run})')
        self.runEvent.set()

    def get_run(self):
        """thread-safe run getter"""
        self.runEvent.wait()
        self.runEvent.clear()
        result = self.run
        self.runEvent.set()
        return result

    def dispatch_opcode(self, instruction):
        """ top-level dispatch"""
        #logging.debug(f'pdp11CPU dispatch_opcode {oct(instruction)} PSW:{oct(self.psw.get_psw())}')
        # *** Redo this based on the table in PDP-11-10 processor manual.pdf II-1-34
        run = True

        if self.cc_ops.is_cc_op(instruction):
            run, operand1, operand2, assembly, report = self.cc_ops.do_cc_op(instruction)

        elif self.br.is_br_op(instruction):
            run, operand1, operand2, assembly, report = self.br.do_br_op(instruction)

        elif self.noopr_ops.is_noopr_op(instruction):
            run, operand1, operand2, assembly, report = self.noopr_ops.do_noopr_op(instruction)

        elif self.ss_ops.is_ss_op(instruction):
            run, operand1, operand2, assembly, report = self.ss_ops.do_ss_op(instruction)

        elif self.rss_ops.is_rss_op(instruction):
            run, operand1, operand2, assembly, report = self.rss_ops.do_rss_op(instruction)

        elif self.ssdd_ops.is_ssdd_op(instruction):
            run, operand1, operand2, assembly, report = self.ssdd_ops.do_ssdd_op(instruction)

        else:
            run, operand1, operand2, assembly, report = self.other_ops.do_other_op(instruction)

        #logging.debug(f'pdp11CPU dispatch_opcode returns run:{run} {assembly}')
        return run, operand1, operand2, assembly, report

    def instruction_cycle(self):
        """Run one PDP11 fetch-decode-execute window_cycle"""
        # fetch opcode and increment program counter
        self.sw.start("instruction_cycle")
        pc = self.reg.get_pc()  # get pc without incrementing
        instruction = self.ram.read_word_from_pc()  # read at pc and increment pc
        run, operand1, operand2, assembly, report = self.dispatch_opcode(instruction)
        # FIX: the Python's debug line used self.pdp11.CPU_cycles, which
        # doesn't exist; the f-string is built before logging.debug checks the
        # level, so every instruction raised AttributeError.
        if logging.getLogger().isEnabledFor(logging.DEBUG):
            logging.debug(f'instruction_cycle: {self.CPU_cycles} {u.oct6(pc)} {u.oct6(instruction)} {u.pad(assembly, 20)};{self.reg.registers_to_string()} NZVC:{self.psw.get_nzvc()}')
        if pc == self.reg.get_pc():
            # FIX: the Python logged "Halting." as an error here, but didn't
            # halt.  A jump to itself is a legitimate wait loop.
            logging.info(f'instruction_cycle: pc was not changed at {oct(pc)}')
        self.sw.stop("instruction_cycle")
        self.executed[instruction] = f'{instruction},{assembly}'
        self.CPU_cycles = self.CPU_cycles + 1
        return run

class pdp11Run():
    """sets up and runs PDP11 emulator"""
    def __init__(self, pdp11):
        """instantiate the PDP11 CPU"""
        logging.info('initializing PDP11 emulator')
        self.pdp11 = pdp11

    def run(self, limit=None):
        """Run the PDP11 until it halts, or for limit instructions,
        cycling the terminal after each instruction.
        FIX: the Python stopped after 100,000 instructions by default."""
        logging.info('run: begin PDP11 emulator')
        logging.info(f'{self.pdp11.reg.registers_to_string()} NZVC:{self.pdp11.psw.get_nzvc()}')

        # start the processor loop
        self.pdp11.set_run(True)
        self.pdp11.CPU_cycles = 0
        self.pdp11.sw.start("CPU")
        while self.pdp11.get_run():
            # FIX: the Python also counted each instruction here, so it
            # counted every instruction twice.
            self.pdp11.set_run(self.pdp11.instruction_cycle())
            # FIX: the Python called terminal.window_cycle(), which Terminal
            # doesn't have.
            self.pdp11.terminal.cycle()
            if limit is not None and self.pdp11.CPU_cycles > limit:
                logging.info('run: instruction limit reached')
                self.pdp11.set_run(False)
        self.pdp11.sw.stop("CPU")

        logging.info('run: stop PDP11 emulator')
        self.pdp11.am.address_mode_report()
        self.pdp11.sw.report()

        run_time = self.pdp11.sw.get_watch("CPU").get_sum() / 1000000000
        logging.info(f'run_time:{run_time}')
        processor_speed = self.pdp11.CPU_cycles / run_time if run_time else 0  # (cycles per second)
        format_processor_speed = '{:5.0f}'.format(processor_speed)
        logging.info(f'self.pdp11.CPU_cycles:{self.pdp11.CPU_cycles}')
        logging.info(f"processor speed: {self.pdp11.CPU_cycles} cycles / {run_time} seconds = {format_processor_speed} instructions per second")
        logging.info('instructions executed:')
        for item in self.pdp11.executed.keys():
            logging.info(self.pdp11.executed[item])
        logging.info('instructions executed report ends')


