/* test_stack.c - translation of test_stack.py */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

static void test_jsr_R5(void)
{
    /* pdp11-40 4-58 */
    logging_info("test_jsr_R5");
    const int R7 = 7, R6 = 6, R5 = 5;
    const int mode1 = 1;

    /* from pdp11-40 4-59; arbitrary values for test: */
    pyint PC = 001000;
    pyint SP = 002000;
    pyint SBR = 003000;
    pyint Data0 = 001234;
    pyint Data1 = 012345;

    Registers_set(&reg, R7, PC);
    Registers_set(&reg, R6, SP);
    Ram_write_word(&ram, SP, Data0);
    Registers_set(&reg, R5, Data1);
    Ram_write_word(&ram, PC + 2, SBR);

    pyint opcode = 004000 | (R5 << 6) | (mode1 << 3) | R7;
    Ram_write_word(&ram, PC, opcode);

    ASSERT(other_ops_is_other_op(&other, opcode));

    Registers_set(&reg, R7, PC + 2);
    other_ops_do_other_op(&other, opcode);

    pyint reg7 = Registers_get(&reg, R7);
    pyint reg6 = Registers_get(&reg, R6);
    pyint reg5 = Registers_get(&reg, R5);
    pyint top = Ram_read_word(&ram, reg6);

    ASSERT_EQ(reg7, PC + 2);
    ASSERT_EQ(reg6, SP - 2);
    ASSERT_EQ(reg5, PC + 2);
    ASSERT_EQ(top, Data1);
}

static void test_rts(void)
{
    logging_info("test_rts");
    const int R7 = 7, R6 = 6, R5 = 5;

    pyint PC = 001002;
    pyint SP = 002000 - 2;
    pyint SBR = 003000;
    pyint Data0 = 001234;
    pyint Data1 = 012345;

    Registers_set(&reg, R7, SBR);
    Registers_set(&reg, R6, SP);
    Ram_write_word(&ram, SP, Data1);
    Ram_write_word(&ram, SP + 2, Data0);

    pyint opcode = 0000200 | R5;
    Ram_write_word(&ram, PC, opcode);

    ASSERT(other_ops_is_other_op(&other, opcode));

    Registers_set(&reg, R7, PC + 2);
    other_ops_do_other_op(&other, opcode);

    pyint reg7 = Registers_get(&reg, R7);
    pyint reg6 = Registers_get(&reg, R6);
    pyint reg5 = Registers_get(&reg, R5);
    pyint top = Ram_read_word(&ram, reg6);

    ASSERT_EQ(reg7, PC);
    ASSERT_EQ(reg6, SP + 2);
    ASSERT_EQ(reg5, Data1);
    ASSERT_EQ(top, Data0);
}

static void test_mark(void) { logging_info("test_mark"); }
static void test_mtps(void) { logging_info("test_mtps"); }

int main(void)
{
    fixture_init();
    RUN(test_jsr_R5);
    RUN(test_rts);
    RUN(test_mark);
    RUN(test_mtps);
    return TEST_SUMMARY("test_stack");
}
