//! The machine: CPU state, memory, and devices, the bus that connects them,
//! and the processor loop (pdp11.py).
//!
//! Instruction groups are `impl Machine` blocks in their own modules
//! ([`br_ops`](crate::br_ops), [`ss_ops`](crate::ss_ops), ...), and
//! [`execute`](Machine::execute) dispatches to them in the Python's order.

use std::collections::HashMap;
use std::io;
use std::path::Path;

use crate::config::{self, Config};
use crate::dl11::{self, Dl11};
use crate::m9301::{self, M9301};
use crate::psw::{self, Psw};
use crate::ram::Ram;
use crate::registers::Registers;
use crate::stopwatches::StopWatches;
use crate::terminal::Terminal;
use crate::{br_ops, cc_ops, noopr_ops, rss_ops, ss_ops, ssdd_ops};

/// What executing one instruction did: whether the CPU keeps running, and
/// the instruction's disassembly.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Executed {
    pub run: bool,
    pub assembly: String,
}

impl Executed {
    pub(crate) fn running(assembly: String) -> Executed {
        Executed {
            run: true,
            assembly,
        }
    }

    pub(crate) fn stopped(assembly: String) -> Executed {
        Executed {
            run: false,
            assembly,
        }
    }
}

pub struct Machine {
    pub reg: Registers,
    pub ram: Ram,
    pub psw: Psw,
    pub dl11: Option<Dl11>,
    pub m9301: Option<M9301>,
    pub terminal: Terminal,
    pub sw: StopWatches,
    /// Instructions executed by the last [`run`](Machine::run).
    pub cpu_cycles: u64,
    /// How often each addressing mode has been used.
    pub mode_counts: [u64; 8],
    /// Set by any access to the I/O page other than the PSW, for comparisons
    /// with simulators that have device registers there.
    pub io_touched: bool,
    /// The last disassembly of each distinct instruction word, and the order
    /// they were first seen in, for the end-of-run report.
    executed: HashMap<u16, String>,
    executed_order: Vec<u16>,
}

impl Default for Machine {
    fn default() -> Machine {
        Machine::new()
    }
}

impl Machine {
    /// A CPU with 64 KB of memory and no devices.
    pub fn new() -> Machine {
        Machine::with_memory(16)
    }

    fn with_memory(bits: u32) -> Machine {
        Machine {
            reg: Registers::new(),
            ram: Ram::new(bits),
            psw: Psw::default(),
            dl11: None,
            m9301: None,
            terminal: Terminal::new(),
            sw: StopWatches::new(),
            cpu_cycles: 0,
            mode_counts: [0; 8],
            io_touched: false,
            executed: HashMap::new(),
            executed_order: Vec::new(),
        }
    }

    /// The PDP-11/03 as the Python builds it: memory sized by
    /// `config_default.json`, the M9301-YA ROM loaded with the PC at its
    /// console emulator, and the console DL11 at 0o177560.  `root` is the
    /// directory holding `config_default.json` and `source/`.
    pub fn boot(root: impl AsRef<Path>) -> io::Result<Machine> {
        let root = root.as_ref();
        let config = Config::load(root.join(config::DEFAULT_FILE))?;
        let bits = config
            .lookup("ram", "bits")
            .and_then(serde_json::Value::as_u64)
            .and_then(|b| u32::try_from(b).ok())
            .ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    "ram.bits missing from the configuration",
                )
            })?;
        let mut m = Machine::with_memory(bits);
        let rom = M9301::new();
        m.load_listing(root.join(m9301::ROM_FILE))?;
        m.reg.set_pc(rom.switch_settings);
        log::info!(
            "M9301 switch_settings:0o{:o}; PC:0o{:o}",
            rom.switch_settings,
            m.reg.pc()
        );
        m.m9301 = Some(rom);
        m.dl11 = Some(Dl11::new(dl11::CONSOLE));
        Ok(m)
    }

    // ------------------------------------------------------------ the bus

    fn in_io_page(&self, address: u16) -> bool {
        u32::from(address) >= self.ram.io_space
    }

    /// A device register's value, if a device answers at `address`.
    fn io_read(&mut self, address: u16) -> Option<u16> {
        self.io_touched |= address < psw::ADDRESS;
        match address {
            psw::ADDRESS => Some(self.psw.get()),
            m9301::SWITCH_ADDRESS => self.m9301.as_ref().map(M9301::read_switches),
            _ => match &mut self.dl11 {
                Some(d) if d.decodes(address) => Some(d.read(address)),
                _ => None,
            },
        }
    }

    /// Writes a device register; false if no device takes writes at
    /// `address`.  (As in the Python, the PSW and the M9301 switches are
    /// read-only on this bus: writes there go to memory.)
    fn io_write(&mut self, address: u16, value: u16) -> bool {
        self.io_touched |= address < psw::ADDRESS;
        match &mut self.dl11 {
            Some(d) if d.decodes(address) => {
                d.write(address, value);
                true
            }
            _ => false,
        }
    }

    /// Reads a word.  The 11/03 ignores bit 0 of a word address.
    pub fn read_word(&mut self, address: u16) -> u16 {
        let address = address & !1;
        if self.in_io_page(address) {
            if let Some(v) = self.io_read(address) {
                return v;
            }
        }
        self.ram.read_word(address.into())
    }

    pub fn write_word(&mut self, address: u16, value: u16) {
        let address = address & !1;
        if self.in_io_page(address) && self.io_write(address, value) {
            return;
        }
        self.ram.write_word(address.into(), value);
    }

    pub fn read_byte(&mut self, address: u16) -> u8 {
        if self.in_io_page(address) {
            if let Some(v) = self.io_read(address) {
                return v as u8;
            }
        }
        self.ram.read_byte(address.into())
    }

    pub fn write_byte(&mut self, address: u16, value: u8) {
        if self.in_io_page(address) && self.io_write(address, value.into()) {
            return;
        }
        self.ram.write_byte(address.into(), value);
    }

    /// Reads the word at the PC and advances the PC past it.
    pub fn fetch(&mut self) -> u16 {
        let word = self.read_word(self.reg.pc());
        self.reg.inc_pc();
        word
    }

    // ---------------------------------------------------------- the stack

    pub fn push(&mut self, value: u16) {
        let sp = self.reg.sp().wrapping_sub(2);
        self.reg.set_sp(sp);
        self.write_word(sp, value);
    }

    pub fn pop(&mut self) -> u16 {
        let sp = self.reg.sp();
        let value = self.read_word(sp);
        self.reg.set_sp(sp.wrapping_add(2));
        value
    }

    /// Pushes the PS and PC, then loads the PC and PS from `vector` and
    /// `vector + 2`.
    pub fn trap(&mut self, vector: u16) {
        self.push(self.psw.get());
        self.push(self.reg.pc());
        let pc = self.read_word(vector);
        self.reg.set_pc(pc);
        let ps = self.read_word(vector + 2);
        self.psw.set(ps);
    }

    // ---------------------------------------------------- the processor

    /// Executes one instruction that has already been fetched.
    pub fn execute(&mut self, instruction: u16) -> Executed {
        type Op = fn(&mut Machine, u16) -> Executed;
        let (group, op): (&'static str, Op) = if cc_ops::is_cc_op(instruction) {
            ("cc", Machine::do_cc_op)
        } else if br_ops::is_br_op(instruction) {
            ("br", Machine::do_br_op)
        } else if noopr_ops::is_noopr_op(instruction) {
            ("noopr", Machine::do_noopr_op)
        } else if ss_ops::is_ss_op(instruction) {
            ("ss", Machine::do_ss_op)
        } else if rss_ops::is_rss_op(instruction) {
            ("rss", Machine::do_rss_op)
        } else if ssdd_ops::is_ssdd_op(instruction) {
            ("ssdd", Machine::do_ssdd_op)
        } else {
            ("other", Machine::do_other_op)
        };
        self.sw.start(group);
        let executed = op(self, instruction);
        self.sw.stop(group);
        executed
    }

    /// One fetch-decode-execute cycle; returns whether the CPU keeps running.
    pub fn instruction_cycle(&mut self) -> bool {
        self.sw.start("instruction_cycle");
        let pc = self.reg.pc();
        let instruction = self.fetch();
        let Executed { run, assembly } = self.execute(instruction);
        if log::log_enabled!(log::Level::Debug) {
            log::debug!(
                "instruction_cycle: {} {pc:06o} {instruction:06o} {assembly:<20};{} NZVC:{}",
                self.cpu_cycles,
                self.reg,
                self.psw.nzvc()
            );
        }
        if pc == self.reg.pc() {
            // A jump to itself is a legitimate wait loop, so this does not halt.
            log::info!("instruction_cycle: pc was not changed at 0o{pc:o}");
        }
        self.sw.stop("instruction_cycle");
        if self.executed.insert(instruction, assembly).is_none() {
            self.executed_order.push(instruction);
        }
        self.cpu_cycles += 1;
        run
    }

    /// Runs until the CPU stops, or for `limit` instructions, passing
    /// characters between the console DL11 and the terminal after each one.
    pub fn run(&mut self, limit: Option<u64>) {
        log::info!("run: begin PDP11 emulator");
        log::info!("{} NZVC:{}", self.reg, self.psw.nzvc());
        self.cpu_cycles = 0;
        self.sw.start("CPU");
        loop {
            let running = self.instruction_cycle();
            if let Some(d) = &mut self.dl11 {
                self.terminal.cycle(d);
            }
            if !running {
                break;
            }
            if limit.is_some_and(|l| self.cpu_cycles > l) {
                log::info!("run: instruction limit reached");
                break;
            }
        }
        self.sw.stop("CPU");
        log::info!("run: stop PDP11 emulator");
        self.report();
    }

    fn report(&self) {
        log::info!("address modes used:");
        for (mode, n) in self.mode_counts.iter().enumerate() {
            log::info!("{mode}:{n}");
        }
        self.sw.report();
        let seconds = self.sw.watch("CPU").map_or(0.0, |w| w.sum.as_secs_f64());
        let speed = if seconds > 0.0 {
            self.cpu_cycles as f64 / seconds
        } else {
            0.0
        };
        log::info!(
            "processor speed: {} cycles / {seconds} seconds = {speed:5.0} instructions per second",
            self.cpu_cycles
        );
        log::info!("instructions executed:");
        for i in &self.executed_order {
            log::info!("{i},{}", self.executed[i]);
        }
        log::info!("instructions executed report ends");
        log::logger().flush();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{br_ops, noopr_ops, other_ops, rss_ops, ss_ops, ssdd_ops};

    /// The "Hello, World!" program from
    /// <http://www.retrocmp.com/how-tos/interfacing-to-a-pdp-1105/146-interfacing-with-a-pdp-1105-test-programs-and-qhello-worldq>
    const HELLO_WORLD: [u16; 13] = [
        0o012702, 0o177564, // 2000 start:  MOV #serial+4,R2  ; R2 points to the DL11 XCSR
        0o012701, 0o002032, // 2004         MOV #string,R1    ; R1 points to the current char
        0o112100, //           2010 nxtchr: MOVB (R1)+,R0     ; load xmt char
        0o001405, //           2012         BEQ done          ; string is terminated with 0
        0o110062, 0o000002, // 2014         MOVB R0,2(R2)     ; write char to transmit buffer
        0o105712, //           2020 wait:   TSTB (R2)         ; character transmitted?
        0o100376, //           2022         BPL wait          ; no, loop
        0o000771, //           2024         BR nxtchr         ; transmit next character
        0o000000, //           2026 done:   HALT
        0o000763, //           2030         BR start
    ];

    #[test]
    fn hello_world() {
        // The Python test opened the VT52 window with no assertion; this
        // captures the output and checks it and the halt.
        let mut m = Machine::new();
        m.dl11 = Some(Dl11::new(dl11::CONSOLE));
        m.terminal.capture();
        m.load_machine_code(&HELLO_WORLD, 0o2000);
        let message = "Hello, World!\nHello, World!\nHello, World!";
        for (i, c) in message.bytes().chain([0]).enumerate() {
            m.write_byte(0o2032 + i as u16, c);
        }
        m.run(Some(100_000));
        assert_eq!(String::from_utf8_lossy(m.terminal.captured()), message);
        assert_eq!(m.reg.pc(), 0o2030, "stopped just past the HALT at 2026");
        assert!(m.cpu_cycles < 100_000, "halted, not stopped by the limit");
    }

    #[test]
    fn boots_the_rom() {
        let mut m = Machine::boot(crate::repo_root()).unwrap();
        assert_eq!(m.reg.pc(), m9301::BASE_ADDRESS);
        assert_eq!(m.read_word(m9301::SWITCH_ADDRESS), m9301::BASE_ADDRESS);
        m.terminal.capture();
        m.run(Some(200_000));
        // The console emulator prints four registers, then its prompt, and
        // waits for a command (the C and Go versions print the same).
        let out = String::from_utf8_lossy(m.terminal.captured()).into_owned();
        assert_eq!(out, "\r\n000000 000000 000000 000000 \r\n$");
    }

    #[test]
    fn psw_is_readable_on_the_bus() {
        let mut m = Machine::new();
        m.psw.set(0o345);
        assert_eq!(m.read_word(psw::ADDRESS), 0o345);
        assert!(!m.io_touched, "the PSW does not count as an I/O access");
        m.read_word(0o177560);
        assert!(m.io_touched);
    }

    #[test]
    fn stack() {
        let mut m = Machine::new();
        m.reg.set_sp(0o1000);
        m.push(0o1234);
        m.push(0o4321);
        assert_eq!(m.reg.sp(), 0o774);
        assert_eq!(m.pop(), 0o4321);
        assert_eq!(m.pop(), 0o1234);
        assert_eq!(m.reg.sp(), 0o1000);
    }

    /// test_implemented: every opcode (instruction >> 6) decodes to some
    /// group, and the groups' names cover the ones they claim.  The Python
    /// logged the survey without assertions.
    #[test]
    fn implemented() {
        let mut groups = std::collections::BTreeMap::<&str, u32>::new();
        for i in 0..0o1777u16 {
            let opcode = i << 6;
            let group = if cc_ops::is_cc_op(opcode) {
                "CC"
            } else if br_ops::is_br_op(opcode) {
                assert!(br_ops::branch_name(opcode & 0o177400).is_some());
                "BR"
            } else if noopr_ops::is_noopr_op(opcode) {
                assert!(noopr_ops::noopr_name(opcode).is_some());
                "NOPR"
            } else if ss_ops::is_ss_op(opcode) {
                assert!(
                    ss_ops::single_operand_name(opcode & 0o107700).is_some(),
                    "0o{opcode:o}"
                );
                "SS"
            } else if rss_ops::is_rss_op(opcode) {
                assert!(rss_ops::rss_name(opcode & 0o077000).is_some());
                "RSS"
            } else if ssdd_ops::is_ssdd_op(opcode) {
                assert!(ssdd_ops::double_operand_name(opcode & 0o170000).is_some());
                "SSDD"
            } else if other_ops::is_other_op(opcode) {
                "other"
            } else {
                "not implemented"
            };
            *groups.entry(group).or_default() += 1;
        }
        assert_eq!(groups["SS"], 16 + 16 + 2); // word and byte opcodes, JMP and SWAB
        assert_eq!(groups["SSDD"], 12 * 64);
        assert_eq!(groups["RSS"], 6 * 8);
    }
}
