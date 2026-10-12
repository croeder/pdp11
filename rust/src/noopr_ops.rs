//! Instructions without operands, 0-6 (pdp11_noopr_ops).

use crate::{Executed, Machine};

pub fn noopr_name(instruction: u16) -> Option<&'static str> {
    Some(match instruction {
        0 => "HALT",
        1 => "WAIT",
        2 => "RTI",
        3 => "BPT",
        4 => "IOT",
        5 => "RESET",
        6 => "RTT",
        _ => return None,
    })
}

pub fn is_noopr_op(instruction: u16) -> bool {
    instruction <= 6
}

impl Machine {
    pub fn do_noopr_op(&mut self, instruction: u16) -> Executed {
        let Some(name) = noopr_name(instruction) else {
            return Executed::stopped("Error: no-operand opcode not found".into());
        };
        let run = match instruction {
            // HALT
            0 => false,
            // WAIT (4-75): there are no interrupts to wait for, so the CPU
            // stops.  (The Python also advanced the PC past the next word.)
            1 => {
                log::info!("WAIT: no interrupts implemented; stopping");
                false
            }
            // RTI (4-69) and RTT (4-70): PC <- (SP)+; PS <- (SP)+.  No trace
            // trap is emulated, so they are the same.
            2 | 6 => {
                let pc = self.pop();
                self.reg.set_pc(pc);
                let ps = self.pop();
                self.psw.set(ps);
                true
            }
            // BPT (4-67): trap through vector 14.
            3 => {
                self.trap(0o14);
                true
            }
            // IOT (4-68): trap through vector 20.
            4 => {
                self.trap(0o20);
                true
            }
            // RESET (4-76): there is no bus to reset.
            _ => true,
        };
        Executed {
            run,
            assembly: name.into(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn halt_and_wait_stop() {
        let mut m = Machine::new();
        assert!(!m.do_noopr_op(0).run);
        assert!(!m.do_noopr_op(1).run);
        assert!(m.do_noopr_op(5).run);
    }

    #[test]
    fn bpt_traps_and_rti_returns() {
        let mut m = Machine::new();
        m.write_word(0o14, 0o5000);
        m.write_word(0o16, 0o340);
        m.reg.set_sp(0o1000);
        m.reg.set_pc(0o2002);
        m.psw.set(0o17);
        m.do_noopr_op(3);
        assert_eq!(m.reg.pc(), 0o5000);
        assert_eq!(m.psw.get(), 0o340);
        assert_eq!(m.reg.sp(), 0o774);
        assert_eq!(m.read_word(0o774), 0o2002, "pushed PC");
        assert_eq!(m.read_word(0o776), 0o17, "pushed PS");
        m.do_noopr_op(2);
        assert_eq!(m.reg.pc(), 0o2002);
        assert_eq!(m.psw.get(), 0o17);
        assert_eq!(m.reg.sp(), 0o1000);
    }
}
