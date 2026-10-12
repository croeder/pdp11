//! Single-operand instructions, `* 050 DD` - `* 067 DD`, plus JMP and SWAB
//! (pdp11_ss_ops).  Semantics follow the DEC processor handbook; the notes
//! say where the Python differed.

use crate::psw::Psw;
use crate::{Executed, Machine, Size};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Op {
    Jmp,
    Swab,
    Clr,
    Com,
    Inc,
    Dec,
    Neg,
    Adc,
    Sbc,
    Tst,
    Ror,
    Rol,
    Asr,
    Asl,
    Sxt,
    /// MARK, MFPI, MTPI, MTPS, MFPD, MTPD, MFPS: not implemented; the
    /// operand's address is computed but nothing is read or written.
    Unimplemented,
}

/// How an instruction uses its operand.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Access {
    ReadWrite,
    /// CLR and SXT write without reading.
    WriteOnly,
    /// TST reads without writing.
    ReadOnly,
    None,
}

impl Op {
    fn access(self) -> Access {
        match self {
            Op::Clr | Op::Sxt => Access::WriteOnly,
            Op::Tst => Access::ReadOnly,
            Op::Unimplemented => Access::None,
            _ => Access::ReadWrite,
        }
    }
}

/// The mnemonic and operation of `instruction & 0o107700`.
fn decode(opcode: u16) -> Option<(&'static str, Op)> {
    use Op::*;
    Some(match opcode {
        0o000100 => ("JMP", Jmp),
        0o000300 => ("SWAB", Swab),
        0o005000 => ("CLR", Clr),
        0o005100 => ("COM", Com),
        0o005200 => ("INC", Inc),
        0o005300 => ("DEC", Dec),
        0o005400 => ("NEG", Neg),
        0o005500 => ("ADC", Adc),
        0o005600 => ("SBC", Sbc),
        0o005700 => ("TST", Tst),
        0o006000 => ("ROR", Ror),
        0o006100 => ("ROL", Rol),
        0o006200 => ("ASR", Asr),
        0o006300 => ("ASL", Asl),
        0o006400 => ("MARK", Unimplemented),
        0o006500 => ("MFPI", Unimplemented),
        0o006600 => ("MTPI", Unimplemented),
        0o006700 => ("SXT", Sxt),
        0o105000 => ("CLRB", Clr),
        0o105100 => ("COMB", Com),
        0o105200 => ("INCB", Inc),
        0o105300 => ("DECB", Dec),
        0o105400 => ("NEGB", Neg),
        0o105500 => ("ADCB", Adc),
        0o105600 => ("SBCB", Sbc),
        0o105700 => ("TSTB", Tst),
        0o106000 => ("RORB", Ror),
        0o106100 => ("ROLB", Rol),
        0o106200 => ("ASRB", Asr),
        0o106300 => ("ASLB", Asl),
        0o106400 => ("MTPS", Unimplemented),
        0o106500 => ("MFPD", Unimplemented),
        0o106600 => ("MTPD", Unimplemented),
        0o106700 => ("MFPS", Unimplemented),
        _ => return None,
    })
}

/// The mnemonic of a single-operand opcode (`instruction & 0o107700`).
pub fn single_operand_name(opcode: u16) -> Option<&'static str> {
    decode(opcode).map(|(name, _)| name)
}

/// `* 000 101 *** ***` or `* 000 110 *** ***`, JMP, or SWAB.
pub fn is_ss_op(instruction: u16) -> bool {
    let group = instruction & 0o007000;
    (instruction & 0o070000 == 0 && (group == 0o005000 || group == 0o006000))
        || instruction & 0o177700 == 0o000100
        || instruction & 0o177700 == 0o000300
}

/// The result of `op` on `d` for an operand of `size`, setting the
/// condition codes.  Byte results keep `d`'s high byte, which matters for a
/// register operand.
fn compute(op: Op, size: Size, d: u16, psw: &mut Psw) -> u16 {
    let (mask, sign) = (size.mask(), size.sign());
    let v = d & mask;
    let carry = psw.c();
    match op {
        // SWAB: N and Z from the new low byte; V and C cleared.
        Op::Swab => {
            let r = d.swap_bytes();
            psw.set_nz(Size::Byte, r & 0o377);
            psw.set_v(false);
            psw.set_c(false);
            return r;
        }
        // SXT: all ones if N is set, else 0; Z = !N; V cleared.  (The Python
        // stored 1 for -1 and kept V.)
        Op::Sxt => {
            let n = psw.n();
            psw.set_z(!n);
            psw.set_v(false);
            return if n { 0o177777 } else { 0 };
        }
        Op::Jmp | Op::Unimplemented => return d,
        _ => {}
    }
    // The result (within the size), V, and C (None: unaffected).  N and Z
    // come from the result.
    let (r, overflow, c): (u16, bool, Option<bool>) = match op {
        Op::Clr => (0, false, Some(false)),
        Op::Tst => (v, false, Some(false)),
        // COM: V cleared, C set
        Op::Com => (!v & mask, false, Some(true)),
        // INC, DEC: V on overflow, C unaffected.  (The Python's V test wrote
        // the mode bits.)
        Op::Inc => {
            let r = v.wrapping_add(1) & mask;
            (r, r == sign, None)
        }
        Op::Dec => {
            let r = v.wrapping_sub(1) & mask;
            (r, r == sign - 1, None)
        }
        // NEG: V if the result is the most negative value, C unless it is
        // 0.  (The Python stored a negative Python int and set neither.)
        Op::Neg => {
            let r = v.wrapping_neg() & mask;
            (r, r == sign, Some(r != 0))
        }
        // ADC, SBC.  (The Python set no condition codes.)
        Op::Adc => {
            let r = v.wrapping_add(carry.into()) & mask;
            (r, carry && r == sign, Some(carry && r == 0))
        }
        Op::Sbc => {
            let r = v.wrapping_sub(carry.into()) & mask;
            (r, carry && r == sign - 1, Some(carry && r == mask))
        }
        // Rotates and shifts set V = N xor C.  (The Python's byte rotates
        // touched the high byte, its ROR took C from the result, its ROL
        // ignored the carry, and its ASL tested bit 45 for C.)
        Op::Ror => {
            let r = v >> 1 | if carry { sign } else { 0 };
            shifted(r, v & 1 != 0, sign)
        }
        Op::Rol => {
            let r = (v << 1 | u16::from(carry)) & mask;
            shifted(r, v & sign != 0, sign)
        }
        Op::Asr => shifted(v >> 1 | v & sign, v & 1 != 0, sign),
        Op::Asl => shifted((v << 1) & mask, v & sign != 0, sign),
        Op::Swab | Op::Sxt | Op::Jmp | Op::Unimplemented => unreachable!("handled above"),
    };
    psw.set_nz(size, r);
    psw.set_v(overflow);
    if let Some(c) = c {
        psw.set_c(c);
    }
    if op == Op::Tst {
        d
    } else {
        size.merge(r, d)
    }
}

/// A rotate or shift result with the bit shifted out as C, and V = N xor C.
fn shifted(r: u16, c: bool, sign: u16) -> (u16, bool, Option<bool>) {
    (r, (r & sign != 0) != c, Some(c))
}

impl Machine {
    pub fn do_ss_op(&mut self, instruction: u16) -> Executed {
        let size = Size::of(instruction);
        let field = instruction & 0o77;
        let Some((name, op)) = decode(instruction & 0o107700) else {
            return Executed::stopped(format!(
                "Error: single operand opcode 0o{instruction:o} not found"
            ));
        };
        if op == Op::Jmp {
            // JMP to a register is illegal; the CPU stops without jumping
            // (the hardware traps; the Python jumped to 0).
            return match self.jump_target(field) {
                Ok(target) => {
                    self.reg
                        .set_pc(target.address().expect("a jump target is in memory"));
                    Executed::running(format!("JMP {target}"))
                }
                Err(register) => Executed::stopped(format!("JMP {register}")),
            };
        }
        let operand = match op.access() {
            Access::WriteOnly | Access::None => self.operand_location(size, field),
            Access::ReadWrite | Access::ReadOnly => self.operand(size, field),
        };
        let result = compute(op, size, operand.value, &mut self.psw);
        if op.access() == Access::ReadWrite || op.access() == Access::WriteOnly {
            self.store(size, &operand, result);
        }
        Executed::running(format!("{name} {operand}"))
    }
}

#[cfg(test)]
mod tests {
    //! test_ss_ops and test_jump
    use super::*;

    #[test]
    fn single_operand() {
        // (name, PSW before, R1 before, instruction, assembly, R1 after, NZVC after)
        let cases = [
            ("CLR", 0, 0o013333, 0o005001, "CLR R1", 0, "0100"),
            ("COM_1", 0, 0o013333, 0o005101, "COM R1", 0o164444, "1001"),
            ("COM_2", 0, 0o164444, 0o005101, "COM R1", 0o013333, "0001"),
            ("COMB_1", 0, 0o000022, 0o105101, "COMB R1", 0o355, "1001"),
            ("INC", 0, 0o013333, 0o005201, "INC R1", 0o013334, "0000"),
            ("INCB", 0, 0o000033, 0o105201, "INCB R1", 0o000034, "0000"),
            ("DEC_1", 0, 0o013333, 0o005301, "DEC R1", 0o013332, "0000"),
            ("DEC_2", 0, 1, 0o005301, "DEC R1", 0, "0100"),
            ("DEC_3", 0, 0, 0o005301, "DEC R1", 0o177777, "1000"), // no overflow
            ("DECB_1", 0, 1, 0o105301, "DECB R1", 0, "0100"),
            (
                "SWAB_1", 0o177777, 0o077777, 0o000301, "SWAB R1", 0o177577, "0000",
            ),
            (
                "SWAB_2", 0o177777, 0o177577, 0o000301, "SWAB R1", 0o077777, "1000",
            ),
            (
                "SWAB_3", 0o177777, 0o000377, 0o000301, "SWAB R1", 0o177400, "0100",
            ),
            (
                "SWAB_4", 0o177777, 0o177400, 0o000301, "SWAB R1", 0o000377, "1000",
            ),
            // C=1 rotates into bit 15 (Python: 0x7F80, "0000")
            (
                "ROR_1", 0o177777, 0xFF00, 0o006001, "ROR R1", 0xFF80, "1010",
            ),
            (
                "ROR_2", 0o177777, 0x00FF, 0o006001, "ROR R1", 0x807F, "1001",
            ),
            // only the low byte rotates (Python: 0x8778, "0011"; 0x7887)
            (
                "RORB_1", 0o177777, 0x0FF0, 0o106001, "RORB R1", 0x0FF8, "1010",
            ),
            (
                "RORB_2", 0o177777, 0xF00F, 0o106001, "RORB R1", 0xF087, "1001",
            ),
            ("ROL", 0o177777, 0xCC33, 0o006101, "ROL R1", 0x9867, "1001"),
            // only the low byte rotates (Python: 0x9966, "0011")
            (
                "ROLB", 0o177777, 0xCC33, 0o106101, "ROLB R1", 0xCC67, "0000",
            ),
            ("ASR", 0o177777, 0xCC33, 0o006201, "ASR R1", 0xE619, "1001"),
            // only the low byte shifts (Python: 0xE619)
            (
                "ASRB", 0o177777, 0xCC33, 0o106201, "ASRB R1", 0xCC19, "0011",
            ),
        ];
        for (name, psw, r1, instruction, assembly, r1_after, nzvc) in cases {
            let mut m = Machine::new();
            m.psw.set(psw);
            m.reg.set(1, r1);
            assert!(is_ss_op(instruction), "{name}");
            let e = m.do_ss_op(instruction);
            assert_eq!(e.assembly, assembly, "{name}");
            assert_eq!(m.reg.get(1), r1_after, "{name}: R1 0o{:o}", m.reg.get(1));
            assert_eq!(m.psw.nzvc(), nzvc, "{name}");
        }
    }

    #[test]
    fn tst_does_not_write() {
        let mut m = Machine::new();
        m.reg.set(1, 0o1000);
        m.write_word(0o1000, 0o100000);
        m.do_ss_op(0o005711); // TST (R1)
        assert_eq!(m.read_word(0o1000), 0o100000);
        assert_eq!(m.psw.nzvc(), "1000");
    }

    fn jmp(mode: u16, register: u16) -> u16 {
        0o000100 | mode << 3 | register
    }

    #[test]
    fn jmp_mode_0_is_illegal() {
        let mut m = Machine::new();
        m.reg.set_pc(0o1020);
        let e = m.do_ss_op(jmp(0, 0));
        assert!(!e.run);
        assert_eq!(m.reg.pc(), 0o1020);
    }

    #[test]
    fn jmp_mode_1() {
        let mut m = Machine::new();
        m.reg.set_pc(0o1000);
        m.reg.set(1, 0o2000);
        m.do_ss_op(jmp(1, 1));
        assert_eq!(m.reg.pc(), 0o2000);
    }

    #[test]
    fn jmp_mode_2() {
        let mut m = Machine::new();
        m.reg.set(1, 0o2000);
        m.do_ss_op(jmp(2, 1));
        assert_eq!(m.reg.pc(), 0o2000);
        assert_eq!(m.reg.get(1), 0o2002);
        m.reg.set_pc(0o2000);
        m.do_ss_op(jmp(2, 7)); // JMP (PC)+ jumps to the word after the instruction
        assert_eq!(m.reg.pc(), 0o2000);
    }

    #[test]
    fn jmp_mode_3_absolute() {
        let mut m = Machine::new();
        m.write_word(0o1000, jmp(3, 7));
        m.write_word(0o1002, 0o2000);
        m.reg.set_pc(0o1002);
        m.do_ss_op(jmp(3, 7));
        assert_eq!(m.reg.pc(), 0o2000);
    }

    #[test]
    fn jmp_mode_6_relative() {
        let mut m = Machine::new();
        let (pc, target) = (0o1000, 0o1200);
        m.write_word(pc, jmp(6, 7));
        m.write_word(pc + 2, target - pc - 4);
        m.reg.set_pc(pc + 2);
        m.do_ss_op(jmp(6, 7));
        assert_eq!(m.reg.pc(), target);
    }

    #[test]
    fn jmp_mode_7_relative_deferred() {
        let mut m = Machine::new();
        let (pc, pointer, target) = (0o1000, 0o1100, 0o3000);
        m.write_word(pc + 2, pointer - pc - 4);
        m.write_word(pointer, target);
        m.reg.set_pc(pc + 2);
        m.do_ss_op(jmp(7, 7));
        assert_eq!(m.reg.pc(), target);
    }
}
