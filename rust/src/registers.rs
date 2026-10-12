//! The general registers R0-R7.  R6 is the stack pointer, R7 the program
//! counter.

use std::fmt;

pub const SP: usize = 6;
pub const PC: usize = 7;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Registers {
    r: [u16; 8],
}

impl Registers {
    pub fn new() -> Registers {
        Registers::default()
    }

    pub fn get(&self, register: usize) -> u16 {
        self.r[register]
    }

    pub fn set(&mut self, register: usize, value: u16) {
        self.r[register] = value;
    }

    pub fn pc(&self) -> u16 {
        self.r[PC]
    }

    pub fn set_pc(&mut self, value: u16) {
        self.r[PC] = value;
    }

    /// Advances the PC by one word and returns the new PC.
    pub fn inc_pc(&mut self) -> u16 {
        self.r[PC] = self.r[PC].wrapping_add(2);
        self.r[PC]
    }

    /// Adds twice a signed 8-bit offset to the PC, as a branch does.
    pub fn branch(&mut self, offset: u8) {
        let words = i16::from(offset as i8);
        self.r[PC] = self.r[PC].wrapping_add((words * 2) as u16);
    }

    pub fn sp(&self) -> u16 {
        self.r[SP]
    }

    pub fn set_sp(&mut self, value: u16) {
        self.r[SP] = value;
    }
}

/// The registers in octal, R0 first: ` 000000 000000 ...`
impl fmt::Display for Registers {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for v in self.r {
            write!(f, " {v:06o}")?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registers() {
        let mut reg = Registers::new();
        for i in 0..7 {
            let v = 0o123223 + i as u16;
            reg.set(i, v);
            assert_eq!(reg.get(i), v);
        }
    }

    #[test]
    fn pc() {
        let mut reg = Registers::new();
        reg.set_pc(0o165250);
        assert_eq!(reg.pc(), 0o165250);
        assert_eq!(reg.inc_pc(), 0o165252);
        assert_eq!(reg.pc(), 0o165252);
    }

    #[test]
    fn branch_offsets() {
        let mut reg = Registers::new();
        reg.set_pc(0o165252);
        reg.branch(0o123);
        assert_eq!(reg.pc(), 0o165250 + 2 + 0o123 * 2, "forward");
        reg.branch(0o277);
        assert_eq!(reg.pc(), 0o165316, "backward");
    }

    #[test]
    fn sp() {
        let mut reg = Registers::new();
        reg.set_sp(0o123221);
        assert_eq!(reg.sp(), 0o123221);
    }

    #[test]
    fn display() {
        let mut reg = Registers::new();
        reg.set(0, 0o1234);
        reg.set_pc(0o173000);
        assert_eq!(
            reg.to_string(),
            " 001234 000000 000000 000000 000000 000000 000000 173000"
        );
    }
}
