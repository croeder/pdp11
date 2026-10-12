//! Condition code operators, 0o240-0o277 (pdp11_cc_ops): bit 4 selects set
//! or clear, bits 3-0 select N, Z, V, C.  0o240 (clear nothing) is NOP.

use crate::psw::NZVC;
use crate::util::oct;
use crate::{Executed, Machine};

const SET: u16 = 0o260;

pub fn is_cc_op(instruction: u16) -> bool {
    (0o240..=0o277).contains(&instruction)
}

impl Machine {
    pub fn do_cc_op(&mut self, instruction: u16) -> Executed {
        let bits = instruction & NZVC;
        let codes = self.psw.get() & NZVC;
        if instruction & SET == SET {
            self.psw.set_nzvc_bits(codes | bits);
            Executed::running(format!("SETCC {}", oct(bits.into())))
        } else {
            self.psw.set_nzvc_bits(codes & !bits);
            Executed::running(format!("CLRCC {}", oct(bits.into())))
        }
    }
}

#[cfg(test)]
mod tests {
    //! test_condition_code_ops (pdp11-40 ref p. 4-79)
    use super::*;

    #[test]
    fn set_opcodes() {
        let mut m = Machine::new();
        for code in 0o1..=0o17 {
            m.psw.set(0);
            let instruction = 0o260 | code;
            assert!(is_cc_op(instruction));
            m.do_cc_op(instruction);
            assert_eq!(m.psw.get() & NZVC, code);
        }
    }

    #[test]
    fn clear_opcodes() {
        let mut m = Machine::new();
        for code in 0..=0o17 {
            m.psw.set(0o17);
            let instruction = 0o240 | code;
            assert!(is_cc_op(instruction));
            m.do_cc_op(instruction);
            assert_eq!(m.psw.get() & NZVC, 0o17 & !code);
        }
    }
}
