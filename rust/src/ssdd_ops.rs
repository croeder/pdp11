//! Double-operand instructions, `01 SS DD` - `06 SS DD` and `11 SS DD` -
//! `16 SS DD` (pdp11_ssdd_ops).  Semantics follow the DEC processor
//! handbook; the notes say where the Python differed.

use crate::psw::Psw;
use crate::{Executed, Machine, Size};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Op {
    Mov,
    Cmp,
    Bit,
    Bic,
    Bis,
    /// ADD (bit 15 clear) or SUB (bit 15 set).  Both are word instructions.
    AddSub,
}

fn op(instruction: u16) -> Option<Op> {
    Some(match instruction & 0o070000 {
        0o010000 => Op::Mov,
        0o020000 => Op::Cmp,
        0o030000 => Op::Bit,
        0o040000 => Op::Bic,
        0o050000 => Op::Bis,
        0o060000 => Op::AddSub,
        _ => return None,
    })
}

/// The mnemonic of `instruction & 0o170000`.
pub fn double_operand_name(opcode: u16) -> Option<&'static str> {
    Some(match opcode {
        0o010000 => "MOV",
        0o020000 => "CMP",
        0o030000 => "BIT",
        0o040000 => "BIC",
        0o050000 => "BIS",
        0o060000 => "ADD",
        0o110000 => "MOVB",
        0o120000 => "CMPB",
        0o130000 => "BITB",
        0o140000 => "BICB",
        0o150000 => "BISB",
        0o160000 => "SUB",
        _ => return None,
    })
}

pub fn is_ssdd_op(instruction: u16) -> bool {
    op(instruction).is_some()
}

/// The result to store in the destination (CMP and BIT store nothing), and
/// the condition codes.
fn compute(op: Op, size: Size, subtract: bool, source: u16, dest: u16, psw: &mut Psw) -> u16 {
    let (mask, sign) = (size.mask(), size.sign());
    let (s, d) = (source & mask, dest & mask);
    match op {
        // MOV: N, Z; V cleared; C unaffected.  MOVB to a register
        // sign-extends; a memory destination takes only the low byte.  (The
        // Python kept the register's old high byte.)
        Op::Mov => {
            psw.set_nz(size, s);
            psw.set_v(false);
            if size == Size::Byte && s & sign != 0 {
                s | 0o177400
            } else {
                s
            }
        }
        // CMP: src - dst, which changes neither.  V: opposite signs and the
        // result has the sign of dst.  C: borrow.  (The Python's V test wrote
        // the mode bits and it never set C.)
        Op::Cmp => {
            let r = s.wrapping_sub(d) & mask;
            psw.set_nz(size, r);
            psw.set_v((s ^ d) & (!d ^ r) & sign != 0);
            psw.set_c(s < d);
            dest
        }
        // BIT: src & dst, which changes neither.  (The Python stored it.)
        Op::Bit => {
            psw.set_nz(size, s & d);
            psw.set_v(false);
            dest
        }
        Op::Bic => {
            let r = !s & d;
            psw.set_nz(size, r);
            psw.set_v(false);
            size.merge(r, dest)
        }
        Op::Bis => {
            let r = s | d;
            psw.set_nz(size, r);
            psw.set_v(false);
            size.merge(r, dest)
        }
        // ADD: V when both operands have one sign and the result the other;
        // C on carry out of bit 15.  SUB: dst - src; V when the operands'
        // signs differ and the result has the sign of src; C on borrow.
        // (The Python tied C to V, missed carries, and never set C on SUB.)
        Op::AddSub => {
            let (r, overflow, carry) = if subtract {
                let r = d.wrapping_sub(s);
                (r, (s ^ d) & (!s ^ r) & 0o100000 != 0, s > d)
            } else {
                let (r, carry) = s.overflowing_add(d);
                (r, !(s ^ d) & (s ^ r) & 0o100000 != 0, carry)
            };
            psw.set_nz(Size::Word, r);
            psw.set_v(overflow);
            psw.set_c(carry);
            r
        }
    }
}

impl Machine {
    pub fn do_ssdd_op(&mut self, instruction: u16) -> Executed {
        let (Some(op), Some(name)) = (op(instruction), double_operand_name(instruction & 0o170000))
        else {
            return Executed::stopped(format!(
                "Error: double operand opcode 0o{instruction:o} not found"
            ));
        };
        // ADD and SUB are word instructions even though SUB has bit 15 set.
        // (The Python fetched SUB's operands as bytes.)
        let size = if op == Op::AddSub {
            Size::Word
        } else {
            Size::of(instruction)
        };
        let source = self.operand(size, (instruction >> 6) & 0o77);
        // MOV does not read its destination (the Python did, which matters
        // for device registers such as the DL11 receive buffer).
        let dest = if op == Op::Mov {
            self.operand_location(size, instruction & 0o77)
        } else {
            self.operand(size, instruction & 0o77)
        };
        let subtract = instruction & 0o100000 != 0;
        let result = compute(op, size, subtract, source.value, dest.value, &mut self.psw);
        if !matches!(op, Op::Cmp | Op::Bit) {
            self.store(size, &dest, result);
        }
        Executed::running(format!("{name} {source},{dest}"))
    }
}

#[cfg(test)]
mod tests {
    //! test_ssdd_ops, and the double-operand cases of test_rss_ops
    use super::*;

    fn ssdd(opcode: u16, rs: u16, rd: u16) -> u16 {
        opcode | rs << 6 | rd
    }

    const fn neg(x: u16) -> u16 {
        x.wrapping_neg()
    }

    const MAX_POS: u16 = 0o077777;

    #[test]
    fn byte_merge() {
        assert_eq!(Size::Word.merge(0xAAAA, 0x5555), 0xAAAA);
        assert_eq!(Size::Byte.merge(0xFF00, 0x00FF), 0);
        assert_eq!(Size::Byte.merge(0x00FF, 0xFF00), 0xFFFF);
        assert_eq!(Size::Byte.merge(0xAAAA, 0x5555), 0x55AA);
        assert_eq!(Size::Byte.merge(0x0000, 0x5555), 0x5500);
    }

    #[test]
    fn register_to_register() {
        // (name, PSW before, source register and value, destination register
        // and value, opcode, assembly, result, NZVC after)
        let cases = [
            (
                "BIC_1",
                0,
                1,
                0xAAAA,
                2,
                0xFFFF,
                0o040000,
                "BIC R1,R2",
                0x5555,
                "0000",
            ),
            (
                "BICB_1",
                0,
                1,
                0xAAAA,
                2,
                0xFFFF,
                0o140000,
                "BICB R1,R2",
                0xFF55,
                "0000",
            ),
            (
                "BIC_2",
                0o177777,
                3,
                0o001234,
                4,
                0o001111,
                0o040000,
                "BIC R3,R4",
                0o000101,
                "0001",
            ),
            (
                "BICB_2",
                0,
                3,
                0o001234,
                4,
                0o001111,
                0o140000,
                "BICB R3,R4",
                0o001101,
                "0000",
            ),
            (
                "MOV_0",
                0o177777,
                3,
                0o123456,
                4,
                0,
                0o010000,
                "MOV R3,R4",
                0o123456,
                "1001",
            ),
            (
                "MOVB_01",
                0o177777,
                3,
                0xA72E,
                4,
                0,
                0o110000,
                "MOVB R3,R4",
                0x002E,
                "0001",
            ),
            // MOVB to a register sign-extends (Python: 0x00AE, 0xA7AE)
            (
                "MOVB_02",
                0o177777,
                3,
                0xA7AE,
                4,
                0,
                0o110000,
                "MOVB R3,R4",
                0xFFAE,
                "1001",
            ),
            (
                "MOVB_03",
                0o177777,
                3,
                0x00AE,
                4,
                0xA700,
                0o110000,
                "MOVB R3,R4",
                0xFFAE,
                "1001",
            ),
            (
                "ADD_PP_P",
                0,
                2,
                486,
                4,
                692,
                0o060000,
                "ADD R2,R4",
                1178,
                "0000",
            ),
            ("ADD_PP_Z", 0, 2, 0, 4, 0, 0o060000, "ADD R2,R4", 0, "0100"),
            // no carry out of bit 15 (Python: "1011")
            (
                "ADD_PP_NV",
                0,
                2,
                MAX_POS - 64,
                4,
                MAX_POS - 64,
                0o060000,
                "ADD R2,R4",
                0o177576,
                "1010",
            ),
            (
                "ADD_NP_PC",
                0,
                2,
                neg(286),
                4,
                384,
                0o060000,
                "ADD R2,R4",
                98,
                "0001",
            ),
            (
                "ADD_NP_ZC",
                0,
                2,
                neg(286),
                4,
                286,
                0o060000,
                "ADD R2,R4",
                0,
                "0101",
            ),
            (
                "ADD_NP_N",
                0,
                2,
                neg(286),
                4,
                3,
                0o060000,
                "ADD R2,R4",
                neg(283),
                "1000",
            ),
            // carry out (Python: "1000")
            (
                "ADD_NN_N",
                0,
                2,
                neg(286),
                4,
                neg(286),
                0o060000,
                "ADD R2,R4",
                neg(572),
                "1001",
            ),
            (
                "ADD_NN_VC",
                0,
                2,
                neg(MAX_POS - 64),
                4,
                neg(MAX_POS - 64),
                0o060000,
                "ADD R2,R4",
                0o202,
                "0011",
            ),
            // from a situation in M9301-YA
            (
                "10_70",
                0,
                2,
                0o177771,
                4,
                0o10,
                0o060000,
                "ADD R2,R4",
                1,
                "0001",
            ),
        ];
        for (name, psw, rs, s, rd, d, opcode, assembly, result, nzvc) in cases {
            let mut m = Machine::new();
            m.psw.set(psw);
            m.reg.set(rs, s);
            m.reg.set(rd, d);
            let instruction = ssdd(opcode, rs as u16, rd as u16);
            assert!(is_ssdd_op(instruction), "{name}");
            let e = m.do_ssdd_op(instruction);
            assert_eq!(e.assembly, assembly, "{name}");
            assert_eq!(m.reg.get(rs), s, "{name}: source unchanged");
            assert_eq!(
                m.reg.get(rd),
                result,
                "{name}: result 0o{:o}",
                m.reg.get(rd)
            );
            assert_eq!(m.psw.nzvc(), nzvc, "{name}");
        }
    }

    #[test]
    fn subtract() {
        // SUB R4,R6: R6 <- R6 - R4.  Names: z zero, p small positive, P big
        // positive, n and N their negatives.
        let (big, small) = (0o076400u16, 0o037200u16); // 32000, 16000
        let (p_, p, z, n, n_) = (big, small, 0u16, neg(small), neg(big));
        let cases = [
            ("zp_N", z, p, "1001"), // borrow (Python: "1000", as for the other borrows)
            ("Pp_P", p_, p, "0000"),
            ("PP_Z", p_, p_, "0100"),
            ("pP_N", p, p_, "1001"),
            ("Nz_N", n_, z, "1000"),
            ("Np_V", n_, p, "0010"), // overflow, no borrow (Python: "0011")
            ("NP_V", n_, p_, "0010"),
            ("zP_N", z, p_, "1001"),
            ("Nn_N", n_, n, "1001"),
            ("NN_Z", n_, n_, "0100"),
            ("nN_P", n, n_, "0000"),
            ("zn_P", z, n, "0001"),
            ("nP_V", n, p_, "0010"),
            ("PN_V", p_, n_, "1011"),
        ];
        for (name, dest, source, nzvc) in cases {
            let mut m = Machine::new();
            m.reg.set(6, dest);
            m.reg.set(4, source);
            let e = m.do_ssdd_op(ssdd(0o160000, 4, 6));
            assert_eq!(e.assembly, "SUB R4,R6");
            assert_eq!(m.reg.get(6), dest.wrapping_sub(source), "{name}");
            assert_eq!(m.psw.nzvc(), nzvc, "{name}");
        }
    }

    #[test]
    fn movb_autoincrement_to_deferred() {
        // 165250 112512 MOVB (R5)+,@R2 from M9301-YA
        let mut m = Machine::new();
        m.psw.set(0o177777);
        m.reg.set(2, 0o500);
        m.write_word(0o500, 0);
        m.reg.set(5, 0o165320);
        m.write_word(0o165320, 0o177777);
        m.reg.set_pc(0o165252);
        let e = m.do_ssdd_op(0o112512);
        assert_eq!(e.assembly, "MOVB (R5)+,@R2");
        assert_eq!(m.reg.get(5), 0o165321);
        assert_eq!(m.read_byte(0o500), 0o377);
        assert_eq!(m.read_byte(0o501), 0, "only the low byte is written");
    }

    #[test]
    fn mov_register_to_autoincrement() {
        // 010322 MOV R3,(R2)+ from M9301-YA
        let mut m = Machine::new();
        m.psw.set(0o177777);
        m.reg.set(3, 0o177777);
        m.reg.set(2, 0o1000);
        let e = m.do_ssdd_op(0o010322);
        assert_eq!(e.assembly, "MOV R3,(R2)+");
        assert_eq!(m.reg.get(2), 0o1002);
        assert_eq!(m.read_word(0o1000), 0o177777);
        assert_eq!(m.psw.nzvc(), "1001");
    }

    #[test]
    fn cmp_and_bit_do_not_write() {
        let mut m = Machine::new();
        m.reg.set(1, 0o1000);
        m.write_word(0o1000, 0o7);
        m.reg.set(0, 0o5);
        m.do_ssdd_op(0o020011); // CMP R0,(R1)
        assert_eq!(m.psw.nzvc(), "1001");
        m.do_ssdd_op(0o030011); // BIT R0,(R1)
        assert_eq!(m.read_word(0o1000), 0o7);
        assert_eq!(m.psw.nzvc(), "0001");
    }
}
