//! RTS and JSR, and what no other group takes (pdp11_other_ops).  An
//! instruction nothing implements stops the CPU.

use crate::util::oct;
use crate::{Executed, Machine};

pub fn is_other_op(instruction: u16) -> bool {
    matches!(
        instruction & 0o177700,
        0o000200 | 0o002000 | 0o004000 | 0o006400 | 0o006500 | 0o006600
    ) || instruction & 0o177000 == 0o004000
}

impl Machine {
    pub fn do_other_op(&mut self, instruction: u16) -> Executed {
        if instruction & 0o177000 == 0o004000 {
            return self.jsr(instruction);
        }
        match instruction & 0o177700 {
            // As in the Python, 0o200-0o237 all decode as RTS.
            0o000200 => self.rts(instruction),
            0o006400 => Executed::running("MARK".into()),
            0o006500 => Executed::running("MFPI".into()),
            0o006600 => Executed::running("MTPI".into()),
            _ => {
                log::info!(
                    "{} is not an implemented instruction; stopping",
                    oct(instruction.into())
                );
                Executed::stopped(oct(instruction.into()))
            }
        }
    }

    /// RTS R: PC <- R; R <- (SP)+
    fn rts(&mut self, instruction: u16) -> Executed {
        let r = usize::from(instruction & 7);
        self.reg.set_pc(self.reg.get(r));
        let value = self.pop();
        self.reg.set(r, value);
        Executed::running(format!("RTS R{r}"))
    }

    /// JSR R,DST: -(SP) <- R; R <- PC; PC <- DST.  SP is decremented before
    /// R is read, so JSR SP,DST pushes the decremented SP, as SIMH and the
    /// hardware do (the Python pushed the old SP).
    fn jsr(&mut self, instruction: u16) -> Executed {
        let r = usize::from((instruction >> 6) & 7);
        let target = match self.jump_target(instruction & 0o77) {
            Ok(target) => target,
            // JSR R,Rn is illegal; the CPU stops (the hardware traps).
            Err(register) => return Executed::stopped(format!("JSR R{r},{register}")),
        };
        let address = target.address().expect("a jump target is in memory");
        let sp = self.reg.sp().wrapping_sub(2);
        self.reg.set_sp(sp);
        let value = self.reg.get(r);
        self.write_word(sp, value);
        self.reg.set(r, self.reg.pc());
        self.reg.set_pc(address);
        Executed::running(format!("JSR R{r},{target}"))
    }
}

#[cfg(test)]
mod tests {
    //! test_stack: JSR and RTS.
    use super::*;

    #[test]
    fn jsr_r5() {
        let mut m = Machine::new();
        let (pc, sp, subroutine, data0, data1) = (0o1000, 0o2000, 0o3000, 0o1234, 0o12345);
        m.reg.set_sp(sp);
        m.write_word(sp, data0);
        m.reg.set(5, data1);
        m.write_word(pc + 2, subroutine);
        let instruction = 0o004000 | 5 << 6 | 3 << 3 | 7; // JSR R5,@#3000
        m.write_word(pc, instruction);
        assert!(is_other_op(instruction));
        m.reg.set_pc(pc + 2);
        let e = m.do_other_op(instruction);
        assert_eq!(e.assembly, "JSR R5,@(R7)+");
        assert_eq!(m.reg.pc(), subroutine);
        assert_eq!(m.reg.sp(), sp - 2);
        assert_eq!(m.reg.get(5), pc + 4, "R5 holds the return address");
        assert_eq!(m.read_word(m.reg.sp()), data1, "old R5 on the stack");
    }

    #[test]
    fn rts() {
        let mut m = Machine::new();
        let (sp, data0, data1, return_to) = (0o1776, 0o1234, 0o12345, 0o1004);
        m.reg.set_sp(sp);
        m.write_word(sp, data1);
        m.write_word(sp + 2, data0);
        m.reg.set(5, return_to);
        let instruction = 0o000205;
        assert!(is_other_op(instruction));
        m.reg.set_pc(0o3002);
        m.do_other_op(instruction);
        assert_eq!(m.reg.pc(), return_to);
        assert_eq!(m.reg.sp(), sp + 2);
        assert_eq!(m.reg.get(5), data1);
        assert_eq!(m.read_word(m.reg.sp()), data0);
    }

    #[test]
    fn jsr_sp_pushes_decremented_sp() {
        let mut m = Machine::new();
        m.reg.set_sp(0o1000);
        m.reg.set(1, 0o2000);
        m.do_other_op(0o004611); // JSR SP,(R1)
        assert_eq!(m.read_word(0o776), 0o776);
        assert_eq!(m.reg.pc(), 0o2000);
    }

    #[test]
    fn jsr_to_register_stops() {
        let mut m = Machine::new();
        m.reg.set_pc(0o1000);
        let e = m.do_other_op(0o004501); // JSR R5,R1
        assert!(!e.run);
        assert_eq!(m.reg.pc(), 0o1000);
    }

    #[test]
    fn unimplemented_stops() {
        let mut m = Machine::new();
        assert!(!m.do_other_op(0o104000).run); // EMT
        assert!(!m.do_other_op(0o000007).run);
    }
}
