//! Branches (pdp11_br_ops): the high byte is the opcode, the low byte a
//! signed word offset from the updated PC.

use crate::psw::Psw;
use crate::util::oct;
use crate::{Executed, Machine};

/// The mnemonic of a branch opcode (`instruction & 0o177400`).
pub fn branch_name(opcode: u16) -> Option<&'static str> {
    Some(match opcode {
        0o000400 => "BR",
        0o001000 => "BNE",
        0o001400 => "BEQ",
        0o002000 => "BGE",
        0o002400 => "BLT",
        0o003000 => "BGT",
        0o003400 => "BLE",
        0o100000 => "BPL",
        0o100400 => "BMI",
        0o101000 => "BHI",
        0o101400 => "BLOS",
        0o102000 => "BVC",
        0o102400 => "BVS",
        0o103000 => "BCC", // = BHIS
        0o103400 => "BCS", // = BLO
        _ => return None,
    })
}

/// Whether a branch is taken, by the DEC definitions.  (The Python's BGE
/// tested N or V instead of N xor V.)
fn taken(opcode: u16, p: Psw) -> bool {
    match opcode {
        0o000400 => true,
        0o001000 => !p.z(),
        0o001400 => p.z(),
        0o002000 => p.n() == p.v(),
        0o002400 => p.n() != p.v(),
        0o003000 => !(p.z() || p.n() != p.v()),
        0o003400 => p.z() || p.n() != p.v(),
        0o100000 => !p.n(),
        0o100400 => p.n(),
        0o101000 => !p.c() && !p.z(),
        0o101400 => p.c() || p.z(),
        0o102000 => !p.v(),
        0o102400 => p.v(),
        0o103000 => !p.c(),
        _ => p.c(), // BCS
    }
}

/// Bit 15 either way, bits 14-12 zero, bits 11-8 a branch code.
pub fn is_br_op(instruction: u16) -> bool {
    instruction & 0o070000 == 0 && branch_name(instruction & 0o177400).is_some()
}

impl Machine {
    pub fn do_br_op(&mut self, instruction: u16) -> Executed {
        let opcode = instruction & 0o177400;
        let offset = (instruction & 0o377) as u8;
        let Some(name) = branch_name(opcode) else {
            return Executed::stopped("Error: branch opcode not found".into());
        };
        if taken(opcode, self.psw) {
            self.reg.branch(offset);
        }
        Executed::running(format!("{name} {}", oct(offset.into())))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conditions() {
        // (opcode, NZVC bits, taken)
        let cases = [
            (0o000400, 0o00, true),
            (0o001000, 0o04, false),
            (0o001400, 0o04, true),
            (0o002000, 0o12, true), // N and V: N xor V is 0
            (0o002000, 0o10, false),
            (0o002400, 0o02, true),
            (0o003000, 0o00, true),
            (0o003000, 0o04, false),
            (0o003400, 0o10, true),
            (0o100000, 0o10, false),
            (0o100400, 0o10, true),
            (0o101000, 0o00, true),
            (0o101000, 0o01, false),
            (0o101400, 0o04, true),
            (0o102000, 0o02, false),
            (0o102400, 0o02, true),
            (0o103000, 0o01, false),
            (0o103400, 0o01, true),
        ];
        for (opcode, bits, expected) in cases {
            let mut m = Machine::new();
            m.psw.set(bits);
            m.reg.set_pc(0o1000);
            assert!(is_br_op(opcode | 0o10));
            m.do_br_op(opcode | 0o10);
            let pc = if expected { 0o1020 } else { 0o1000 };
            assert_eq!(
                m.reg.pc(),
                pc,
                "{} with NZVC {:04b}",
                branch_name(opcode).unwrap(),
                bits
            );
        }
    }

    #[test]
    fn backward() {
        let mut m = Machine::new();
        m.reg.set_pc(0o1000);
        let e = m.do_br_op(0o000776); // BR .-2
        assert_eq!(m.reg.pc(), 0o774);
        assert_eq!(e.assembly, "BR 0o376");
    }

    #[test]
    fn not_branches() {
        assert!(!is_br_op(0o000000));
        assert!(!is_br_op(0o004000)); // JSR
        assert!(!is_br_op(0o104000)); // EMT
        assert!(!is_br_op(0o010400)); // MOV
    }
}
