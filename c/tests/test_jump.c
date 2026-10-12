/* test_jump.c - translation of test_jump.py (test JMP)
 *
 * pdp11-40.pdf page 4-56: PC<-(dst); bits 15-6 are 0o0001; bits 5-0 are mode, register.
 * JMP with mode 0 causes "illegal instruction" condition. */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

enum { R0, R1, R2, R3, R4, R5, R6, R7 };
enum { mode0, mode1, mode2, mode3, mode4, mode5, mode6, mode7 };

static pyint DD(int mode, int register_) { return (pyint)(mode << 3 | register_); }
static pyint JMP_op(int modeD, int regD) { return 0000100 | DD(modeD, regD); }

static void test_jmp_mode_0(void)
{
    logging_info("test_jmp_mode_0 register - illegal");
    pyint pc = 01020;
    pyint instruction = JMP_op(mode0, R0);
    Registers_set_pc(&reg, pc, "test_jmp_mode_1");
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    OpResult r = ss_ops_do_ss_op(&ss, instruction);
    /* JMP to a register is illegal: the CPU stops without jumping
     * (the Python jumped to 0; real hardware traps, which is not emulated) */
    ASSERT_EQ(r.run, 0);
    ASSERT_EQ(Registers_get_pc(&reg), pc);
}

static void test_jmp_mode_1(void)
{
    /* Register Indirect, jumps to wherever the register points - JMP (R1) */
    logging_info("test_jmp_mode_1 immediate");
    pyint pc = 01000, target = 02000;
    pyint instruction = JMP_op(mode1, R1);
    Registers_set_pc(&reg, pc, "test_jmp_mode_1");
    Registers_set(&reg, R1, target);
    Ram_write_word(&ram, pc, instruction);
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), target);
}

static void test_jmp_mode_2_pc(void)
{
    logging_info("test_jmp_mode_2 immediate");
    pyint pc = 01000, target = 02000;
    pyint instruction = JMP_op(mode2, R7);
    Registers_set_pc(&reg, pc, "test_jmp_mode_2");
    Registers_set(&reg, R7, target);
    Ram_write_word(&ram, pc, instruction);
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), target);
}

static void test_jmp_mode_2_r(void)
{
    logging_info("test_jmp_mode_2 immediate");
    pyint pc = 01000, target = 02000;
    pyint instruction = JMP_op(mode2, R1);
    Registers_set_pc(&reg, pc, "test_jmp_mode_2");
    Registers_set(&reg, R1, target);
    Ram_write_word(&ram, pc, instruction);
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), target);
    ASSERT_EQ(Registers_get(&reg, R1), target + 2);
}

static void test_jmp_mode_3(void)
{
    /* Mode 3, Autoincrement Indirect - JMP @(R1)+
     * An absolute jump: JMP @#FOO results in mode 3, register 7,
     * and the second word contains the absolute address of FOO. */
    logging_info("test_jmp_mode_3 ");
    pyint pc = 01000, target = 02000;
    pyint instruction = JMP_op(mode3, R7);
    Ram_write_word(&ram, pc, instruction);
    Ram_write_word(&ram, pc + 2, target);
    Registers_set_pc(&reg, pc + 2, "test_jmp_mode_3");
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), target);
}

static void test_jump_mode_6(void)
{
    /* Mode 6, Indexed - JMP 20(PC) */
    logging_info("test_jump_mode_6 Relative");
    pyint pc = 0001000, target = 0001200;
    pyint relative_address = target - pc - 4;
    pyint instruction = JMP_op(mode6, R7);
    Ram_write_word(&ram, pc, instruction);
    Ram_write_word(&ram, pc + 2, relative_address);
    Registers_set_pc(&reg, pc + 2, "test_jump_mode_6");
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), target);
}

static void test_jump_mode_7(void)
{
    /* Mode 7, Index Indirect - JMP @20(PC).  (As in the Python, this test runs CLR @A.) */
    logging_info("test_jump_mode_7 Relative Deferred");
    pyint instruction = 0005077;
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    pyint a = 0000020, pc = 01020, pointer = 01044, address = 01060;
    Ram_write_word(&ram, pc, instruction);
    Ram_write_word(&ram, pc + 2, a);
    Ram_write_word(&ram, pointer, address);
    Ram_write_word(&ram, address, 0100001);
    Registers_set_pc(&reg, pc, "test_PC_mode_7");
    Registers_set_pc(&reg, pc + 2, "test_PC_mode_7");
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), 01024);
    ASSERT_EQ(Ram_read_word(&ram, address), 00);
    ASSERT_STR(nzvc(), "0100");
}

int main(void)
{
    fixture_init();
    RUN(test_jmp_mode_0);
    RUN(test_jmp_mode_1);
    RUN(test_jmp_mode_2_pc);
    RUN(test_jmp_mode_2_r);
    RUN(test_jmp_mode_3);
    RUN(test_jump_mode_6);
    RUN(test_jump_mode_7);
    return TEST_SUMMARY("test_jump");
}
