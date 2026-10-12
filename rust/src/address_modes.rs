//! Operand addressing: the eight modes, applied to a 6-bit mode/register
//! field.
//!
//! | mode | syntax  | operand is at                                      |
//! |------|---------|----------------------------------------------------|
//! | 0    | Rn      | the register                                       |
//! | 1    | @Rn     | the address in Rn                                  |
//! | 2    | (Rn)+   | the address in Rn; Rn advances                     |
//! | 3    | @(Rn)+  | the address at the address in Rn; Rn advances by 2 |
//! | 4    | -(Rn)   | Rn steps back, then the address in Rn              |
//! | 5    | @-(Rn)  | Rn steps back by 2, then the address at Rn         |
//! | 6    | X(Rn)   | Rn plus the next instruction word X                |
//! | 7    | @X(Rn)  | the address at Rn plus X                           |
//!
//! Byte operands step R0-R5 by 1 and SP and PC by 2.

use std::fmt;

use crate::util::oct;
use crate::{Machine, Size};

/// Where an operand lives.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Location {
    Register(usize),
    Memory(u16),
}

/// A resolved operand: where it is and, if it was read, its value.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Operand {
    pub mode: u16,
    pub register: usize,
    /// The index word X of modes 6 and 7.
    pub index: Option<u16>,
    pub location: Location,
    pub value: u16,
}

impl Operand {
    pub fn address(&self) -> Option<u16> {
        match self.location {
            Location::Memory(a) => Some(a),
            Location::Register(_) => None,
        }
    }
}

/// The operand in assembler syntax, such as `(R5)+` or `@0o20(R7)`.
impl fmt::Display for Operand {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let r = self.register;
        let x = self
            .index
            .map(|x| oct((x as i16).into()))
            .unwrap_or_default();
        match self.mode {
            0 => write!(f, "R{r}"),
            1 => write!(f, "@R{r}"),
            2 => write!(f, "(R{r})+"),
            3 => write!(f, "@(R{r})+"),
            4 => write!(f, "-(R{r})"),
            5 => write!(f, "@-(R{r})"),
            6 => write!(f, "{x}(R{r})"),
            _ => write!(f, "@{x}(R{r})"),
        }
    }
}

impl Machine {
    /// Resolves a mode/register field, applying its side effects on the
    /// registers.  With `fetch`, the operand is read; without, only its
    /// location is found (for destinations that are only written, so a
    /// device register's read side effect is avoided).
    fn resolve(&mut self, size: Size, field: u16, fetch: bool) -> Operand {
        let mode = (field >> 3) & 7;
        let r = usize::from(field & 7);
        self.mode_counts[mode as usize] += 1;
        let step = if size == Size::Byte && r < 6 { 1 } else { 2 };
        let mut index = None;
        let address = match mode {
            0 => {
                let value = self.reg.get(r);
                return Operand {
                    mode,
                    register: r,
                    index,
                    location: Location::Register(r),
                    value,
                };
            }
            1 => self.reg.get(r),
            2 => {
                let a = self.reg.get(r);
                self.reg.set(r, a.wrapping_add(step));
                a
            }
            3 => {
                let a = self.reg.get(r);
                self.reg.set(r, a.wrapping_add(2));
                self.read_word(a)
            }
            4 => {
                let a = self.reg.get(r).wrapping_sub(step);
                self.reg.set(r, a);
                a
            }
            5 => {
                let a = self.reg.get(r).wrapping_sub(2);
                self.reg.set(r, a);
                self.read_word(a)
            }
            6 => {
                let x = self.fetch();
                index = Some(x);
                self.reg.get(r).wrapping_add(x)
            }
            _ => {
                let x = self.fetch();
                index = Some(x);
                let pointer = self.reg.get(r).wrapping_add(x);
                self.read_word(pointer)
            }
        };
        let value = match (fetch, size) {
            (false, _) => 0,
            (true, Size::Word) => self.read_word(address),
            (true, Size::Byte) => self.read_byte(address).into(),
        };
        Operand {
            mode,
            register: r,
            index,
            location: Location::Memory(address),
            value,
        }
    }

    /// Resolves and reads an operand.
    pub fn operand(&mut self, size: Size, field: u16) -> Operand {
        self.resolve(size, field, true)
    }

    /// Resolves an operand without reading it.  A register operand's value
    /// is still filled in.
    pub fn operand_location(&mut self, size: Size, field: u16) -> Operand {
        self.resolve(size, field, false)
    }

    /// The target of a JMP or JSR: the operand's address.  Mode 0 has none
    /// (on the hardware it traps, which is not emulated).
    pub fn jump_target(&mut self, field: u16) -> Result<Operand, String> {
        if field & 0o70 == 0 {
            self.mode_counts[0] += 1;
            return Err(format!("R{}", field & 7));
        }
        Ok(self.resolve(Size::Word, field, false))
    }

    /// Stores a result where an operand came from.
    pub fn store(&mut self, size: Size, operand: &Operand, result: u16) {
        match (operand.location, size) {
            (Location::Register(r), _) => self.reg.set(r, result),
            (Location::Memory(a), Size::Byte) => self.write_byte(a, result as u8),
            (Location::Memory(a), Size::Word) => self.write_word(a, result),
        }
    }
}

#[cfg(test)]
mod tests {
    //! test_address_modes: each mode as the source or destination of ADD
    //! (and INC and CLR for the PC modes).
    use crate::Machine;

    fn add(mode_s: u16, reg_s: u16, mode_d: u16, reg_d: u16) -> u16 {
        0o060000 | (mode_s << 3 | reg_s) << 6 | mode_d << 3 | reg_d
    }

    fn run(m: &mut Machine, instruction: u16) {
        assert!(crate::ssdd_ops::is_ssdd_op(instruction));
        m.do_ssdd_op(instruction);
    }

    #[test]
    fn mode_0() {
        let mut m = Machine::new();
        m.reg.set(1, 3);
        m.reg.set(2, 1);
        run(&mut m, add(0, 1, 0, 2));
        assert_eq!(m.reg.get(2), 4);
        assert_eq!(m.psw.nzvc(), "0000");
    }

    #[test]
    fn mode_1_source() {
        let mut m = Machine::new();
        let (address, a, b) = (0o2764, 0o101, 0o2136);
        m.write_word(address, a);
        m.reg.set(1, address);
        m.reg.set(2, b);
        run(&mut m, add(1, 1, 0, 2));
        assert_eq!(m.reg.get(2), a + b);
        assert_eq!(m.psw.nzvc(), "0000");
    }

    #[test]
    fn mode_1_destination() {
        let mut m = Machine::new();
        let (address, a, b) = (0o2764, 0o101, 0o2136);
        m.write_word(address, a);
        m.reg.set(2, address);
        m.reg.set(1, b);
        run(&mut m, add(0, 1, 1, 2));
        assert_eq!(m.read_word(address), a + b);
    }

    #[test]
    fn mode_1_both() {
        let mut m = Machine::new();
        let (address_a, a, address_b, b) = (0o2762, 0o101, 0o1234, 0o2136);
        m.write_word(address_a, a);
        m.reg.set(1, address_a);
        m.write_word(address_b, b);
        m.reg.set(2, address_b);
        run(&mut m, add(1, 1, 1, 2));
        assert_eq!(m.read_word(address_b), a + b);
        assert_eq!(m.psw.nzvc(), "0000");
    }

    #[test]
    fn mode_2_source() {
        let mut m = Machine::new();
        let (address, a, b) = (0o2762, 0o101, 0o2136);
        m.write_word(address, a);
        m.reg.set(1, address);
        m.reg.set(2, b);
        run(&mut m, add(2, 1, 0, 2));
        assert_eq!(m.reg.get(2), a + b);
        assert_eq!(m.reg.get(1), address + 2);
    }

    #[test]
    fn mode_2_destination() {
        let mut m = Machine::new();
        let (a, b, address) = (0o101, 0o2136, 0o2762);
        m.reg.set(1, a);
        m.write_word(address, b);
        m.reg.set(2, address);
        run(&mut m, add(0, 1, 2, 2));
        assert_eq!(m.read_word(address), a + b);
        assert_eq!(m.reg.get(2), address + 2);
    }

    #[test]
    fn mode_2_both() {
        let mut m = Machine::new();
        let (a, b, source, destination) = (0o101, 0o2136, 0o2762, 0o1312);
        m.reg.set(1, source);
        m.write_word(source, a);
        m.reg.set(2, destination);
        m.write_word(destination, b);
        run(&mut m, add(2, 1, 2, 2));
        assert_eq!(m.read_word(destination), a + b);
        assert_eq!(m.reg.get(1), source + 2);
        assert_eq!(m.reg.get(2), destination + 2);
    }

    #[test]
    fn mode_3_source() {
        let mut m = Machine::new();
        let (pointer, address, a, b) = (0o2762, 0o3000, 0o101, 0o2136);
        m.write_word(address, a);
        m.write_word(pointer, address);
        m.reg.set(1, pointer);
        m.reg.set(2, b);
        run(&mut m, add(3, 1, 0, 2));
        assert_eq!(m.reg.get(2), a + b);
        assert_eq!(m.reg.get(1), pointer + 2);
    }

    #[test]
    fn mode_4() {
        let mut m = Machine::new();
        let (address, a, b) = (0o2762, 0o101, 0o2136);
        m.write_word(address, a);
        m.reg.set(1, address + 2);
        m.reg.set(2, b);
        run(&mut m, add(4, 1, 0, 2));
        assert_eq!(m.reg.get(2), a + b);
        assert_eq!(m.reg.get(1), address);
    }

    #[test]
    fn mode_5() {
        let mut m = Machine::new();
        let (operand_a, address_a, pointer_a, operand_b) = (0o101, 0o2762, 0o1172, 0o2136);
        m.write_word(address_a, operand_a);
        m.write_word(pointer_a, address_a);
        m.reg.set(1, pointer_a + 2);
        m.reg.set(2, operand_b);
        run(&mut m, add(5, 1, 0, 2));
        assert_eq!(m.reg.get(2), operand_a + operand_b);
        assert_eq!(m.reg.get(1), pointer_a);
    }

    #[test]
    fn mode_6_index() {
        let mut m = Machine::new();
        let instruction = add(6, 1, 0, 2);
        let (at, x, a, b) = (0o2762, 0o2050, 0o100, 0o2136);
        m.write_word(at, instruction);
        m.write_word(at + 2, x);
        m.reg.set(1, a);
        m.write_word(x + a, a);
        m.reg.set(2, b);
        m.reg.set_pc(at + 2);
        run(&mut m, instruction);
        assert_eq!(m.reg.get(2), a + b);
        assert_eq!(m.reg.pc(), at + 4);
    }

    #[test]
    fn mode_7_index_deferred() {
        let mut m = Machine::new();
        let instruction = add(7, 1, 0, 2);
        let (at, x, a, address, b) = (0o2762, 0o2050, 0o100, 0o5432, 0o2136);
        m.write_word(at, instruction);
        m.write_word(at + 2, x);
        m.reg.set(1, a);
        m.write_word(address, a);
        m.write_word(x + a, address);
        m.reg.set(2, b);
        m.reg.set_pc(at + 2);
        run(&mut m, instruction);
        assert_eq!(m.reg.get(2), a + b);
    }

    #[test]
    fn pc_mode_2_immediate() {
        // ADD #10,R0
        let mut m = Machine::new();
        let instruction = add(2, 7, 0, 0);
        assert_eq!(instruction, 0o062700);
        m.write_word(0o1020, instruction);
        m.write_word(0o1022, 0o10);
        m.reg.set(0, 0o20);
        m.reg.set_pc(0o1022);
        run(&mut m, instruction);
        assert_eq!(m.reg.get(0), 0o30);
        assert_eq!(m.reg.pc(), 0o1024);
        assert_eq!(m.psw.nzvc(), "0000");
    }

    #[test]
    fn pc_mode_3_absolute() {
        // ADD @#2000,R3
        let mut m = Machine::new();
        let instruction = add(3, 7, 0, 3);
        assert_eq!(instruction, 0o063703);
        let (a, b, pc, address) = (0o300, 0o500, 0o20, 0o2000);
        m.write_word(pc, instruction);
        m.write_word(pc + 2, address);
        m.write_word(address, a);
        m.reg.set(3, b);
        m.reg.set_pc(pc + 2);
        run(&mut m, instruction);
        assert_eq!(m.reg.get(3), a + b);
        assert_eq!(m.reg.pc(), pc + 4);
    }

    #[test]
    fn pc_mode_6_relative() {
        // INC A: see p. 3-16 of the LSI-11 PDP-11/03 Processor Handbook
        let mut m = Machine::new();
        let instruction = 0o005267;
        assert!(crate::ss_ops::is_ss_op(instruction));
        let (a, pc, address) = (0o54, 0o1020, 0o1100);
        m.write_word(pc, instruction);
        m.write_word(pc + 2, a);
        m.write_word(address, 0);
        m.reg.set_pc(pc + 2);
        m.do_ss_op(instruction);
        assert_eq!(m.reg.pc(), 0o1024);
        assert_eq!(m.read_word(address), 1);
        assert_eq!(m.psw.nzvc(), "0000");
    }

    #[test]
    fn pc_mode_7_relative_deferred() {
        // CLR @A: see p. 3-16 of the LSI-11 PDP-11/03 Processor Handbook
        let mut m = Machine::new();
        let instruction = 0o005077;
        assert!(crate::ss_ops::is_ss_op(instruction));
        let (a, pc, pointer, address) = (0o20, 0o1020, 0o1044, 0o1060);
        m.write_word(pc, instruction);
        m.write_word(pc + 2, a);
        m.write_word(pointer, address);
        m.write_word(address, 0o100001);
        m.reg.set_pc(pc + 2);
        let e = m.do_ss_op(instruction);
        assert_eq!(e.assembly, "CLR @0o20(R7)");
        assert_eq!(m.reg.pc(), 0o1024);
        assert_eq!(m.read_word(address), 0);
        assert_eq!(m.psw.nzvc(), "0100");
    }

    #[test]
    fn byte_autoincrement_steps() {
        let mut m = Machine::new();
        m.reg.set(1, 0o1000);
        m.reg.set(6, 0o1000);
        m.operand(crate::Size::Byte, 0o21);
        m.operand(crate::Size::Byte, 0o26);
        assert_eq!(m.reg.get(1), 0o1001, "R1 steps by 1");
        assert_eq!(m.reg.get(6), 0o1002, "SP steps by 2");
    }

    #[test]
    fn negative_index_disassembles_signed() {
        let mut m = Machine::new();
        m.write_word(0o1000, 0o177776);
        m.reg.set_pc(0o1000);
        let o = m.operand(crate::Size::Word, 0o61);
        assert_eq!(o.to_string(), "-0o2(R1)");
    }
}
