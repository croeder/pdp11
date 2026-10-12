/* test_address_modes.c - translation of test_address_modes.py */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

static pyint SS(int mode, int register_) { return (pyint)((mode << 3 | register_) << 6); }
static pyint DD(int mode, int register_) { return (pyint)(mode << 3 | register_); }

static pyint ADD(int modeS, int regS, int modeD, int regD)
{
    return 0060000 | SS(modeS, regS) | DD(modeD, regD);
}

static void do_ssdd_op_report(pyint instruction)
{
    OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);
    logging_info("%s", r.assembly);
}

/* ***************************************** Mode 0 */

static void test_mode_0_SD(void)
{
    logging_info("test_mode_0 register source destination");
    pyint a = 3, b = 1;
    PSW_set_psw(&psw, .psw = 0);
    Registers_set(&reg, 1, a);
    Registers_set(&reg, 2, b);
    pyint instruction = ADD(0, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), a + b);
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 1 Register Deferred */

static void test_mode_1_S(void)
{
    logging_info("test_mode_1 register deferred Source");
    pyint address = 02764, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 1, address);
    Registers_set(&reg, 2, b);
    pyint instruction = ADD(1, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), a + b);
    ASSERT_STR(nzvc(), "0000");
}

static void test_mode_1_D(void)
{
    logging_info("test_mode_1 register deferred Destination");
    pyint address = 02764, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 2, address);
    Registers_set(&reg, 1, b);
    pyint instruction = ADD(0, 1, 1, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Ram_read_word(&ram, address), a + b);
    ASSERT_STR(nzvc(), "0000");
}

static void test_mode_1_SD(void)
{
    logging_info("test_mode_1 register deferred Source Destination");
    pyint addressA = 02762, A = 0101, addressB = 01234, B = 02136;
    Ram_write_word(&ram, addressA, A);
    Registers_set(&reg, 1, addressA);
    Ram_write_word(&ram, addressB, B);
    Registers_set(&reg, 2, addressB);
    pyint instruction = ADD(1, 1, 1, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Ram_read_word(&ram, addressB), A + B);
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 2 Autoincrement */

static void test_mode_2_S(void)
{
    logging_info("test_mode_2 autoincrement source");
    pyint address = 02762, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 1, address);
    Registers_set(&reg, 2, b);
    pyint instruction = ADD(2, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), a + b);
    ASSERT_EQ(address + 2, Registers_get(&reg, 1));
    ASSERT_STR(nzvc(), "0000");
}

static void test_mode_2_D(void)
{
    logging_info("test_mode_2 autoincrement destination");
    pyint a = 0101, b = 02136, address = 02762;
    Registers_set(&reg, 1, a);
    Ram_write_word(&ram, address, b);
    Registers_set(&reg, 2, address);
    pyint expected_sum = a + b;
    pyint instruction = ADD(0, 1, 2, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(expected_sum, Ram_read_word(&ram, address));
    ASSERT_EQ(address + 2, Registers_get(&reg, 2));
    ASSERT_STR(nzvc(), "0000");
}

static void test_mode_2_SD(void)
{
    logging_info("test_mode_2 autoincrement source destination");
    pyint a = 0101, b = 02136, source_address = 02762, destination_address = 01312;
    Registers_set(&reg, 1, source_address);
    Ram_write_word(&ram, source_address, a);
    Registers_set(&reg, 2, destination_address);
    Ram_write_word(&ram, destination_address, b);
    pyint expected_sum = a + b;
    pyint instruction = ADD(2, 1, 2, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(expected_sum, Ram_read_word(&ram, destination_address));
    ASSERT_EQ(source_address + 2, Registers_get(&reg, 1));
    ASSERT_EQ(destination_address + 2, Registers_get(&reg, 2));
}

/* ***************************************** Mode 3 Autoincrement Deferred */

static void test_mode_3_S(void)
{
    /* As in the Python, this test uses mode 2 for the source. */
    logging_info("test_mode_3 autoincrement deferred source");
    pyint address = 02762, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 1, address);
    Registers_set(&reg, 2, b);
    pyint instruction = ADD(2, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), a + b);
    ASSERT_EQ(address + 2, Registers_get(&reg, 1));
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 4 Autodecrement -(Rn) */

static void test_mode_4(void)
{
    logging_info("test_mode_4 autodecrement source");
    pyint address = 02762, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 1, address + 2);
    Registers_set(&reg, 2, b);
    pyint expected_sum = a + b;
    pyint instruction = ADD(4, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_EQ(address, Registers_get(&reg, 1));
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 5 Autodecrement Deferred */

static void test_mode_5(void)
{
    logging_info("test_mode_5 autodecrement deferred source");
    pyint operanda = 0101, addressa = 02762, pointera = 01172, operandb = 02136;
    Ram_write_word(&ram, addressa, operanda);
    Ram_write_word(&ram, pointera, addressa);
    Registers_set(&reg, 1, pointera + 2);
    Registers_set(&reg, 2, operandb);
    pyint expected_sum = operanda + operandb;
    pyint instruction = ADD(5, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_EQ(pointera, Registers_get(&reg, 1));
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 6 Index X(Rn) */

static void test_mode_6(void)
{
    logging_info("test_mode_6 index source");
    pyint instruction = ADD(6, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    pyint instruction_address = 02762;
    Ram_write_word(&ram, instruction_address, instruction);
    pyint X = 02050;
    Ram_write_word(&ram, instruction_address + 2, X);
    pyint operanda = 0100;
    Registers_set(&reg, 1, operanda);
    pyint operand_address = X + operanda;
    Ram_write_word(&ram, operand_address, operanda);
    pyint operandb = 02136;
    Registers_set(&reg, 2, operandb);
    pyint expected_sum = operanda + operandb;
    Registers_set_pc(&reg, instruction_address + 2, "test_mode_6");
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_STR(nzvc(), "0000");
}

/* ***************************************** Mode 7 index Deferred @X(Rn) */

static void test_mode_7(void)
{
    logging_info("test_mode_7 index deferred source");
    pyint instruction = ADD(7, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    pyint instruction_address = 02762;
    Ram_write_word(&ram, instruction_address, instruction);
    pyint X = 02050;
    Ram_write_word(&ram, instruction_address + 2, X);
    pyint operanda = 0100;
    Registers_set(&reg, 1, operanda);
    pyint operand_pointer = X + operanda;
    pyint operand_address = 05432;
    Ram_write_word(&ram, operand_address, operanda);
    Ram_write_word(&ram, operand_pointer, operand_address);
    pyint operandb = 02136;
    Registers_set(&reg, 2, operandb);
    pyint expected_sum = operanda + operandb;
    Registers_set_pc(&reg, instruction_address + 2, "test_mode_7");
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_STR(nzvc(), "0000");
}

/* Program Counter and Address Modes */

static void test_PC_mode_0(void)
{
    logging_info("test_PC_mode_0");
    pyint a = 3, b = 1;
    PSW_set_psw(&psw, .psw = 0);
    Registers_set(&reg, 1, a);
    Registers_set(&reg, 2, b);
    pyint instruction = ADD(0, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), a + b);
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_2(void)
{
    logging_info("test_PC_mode_2 immediate");
    /* ADD #10,R0 */
    pyint instruction = ADD(2, 7, 0, 0);
    ASSERT_EQ(instruction, 0062700);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    pyint address = 01020;
    Ram_write_word(&ram, address, instruction);
    Ram_write_word(&ram, address + 2, 0000010);
    Registers_set(&reg, 0, 0000020);
    Registers_set_pc(&reg, address, "test_PC_mode_2");
    Registers_set_pc(&reg, address + 2, "test_PC_mode_2");
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 0), 0000030);
    ASSERT_EQ(Registers_get_pc(&reg), 01024);
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_3(void)
{
    logging_info("test_PC_mode_3 absolute");
    /* ADD @#2000,R3 */
    pyint instruction = ADD(3, 7, 0, 3);
    ASSERT_EQ(instruction, 0063703);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    pyint a = 000300, b = 000500, pc = 020, address = 02000;
    Ram_write_word(&ram, pc, instruction);
    Ram_write_word(&ram, pc + 2, address);
    Ram_write_word(&ram, address, 000300);
    Registers_set(&reg, 3, b);
    Registers_set_pc(&reg, pc, "test_PC_mode_3");
    Registers_set_pc(&reg, pc + 2, "test_PC_mode_3");
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 3), a + b);
    ASSERT_EQ(Registers_get_pc(&reg), pc + 4);
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_4(void)
{
    logging_info("test_PC_mode_4");
    pyint address = 02762, a = 0101, b = 02136;
    Ram_write_word(&ram, address, a);
    Registers_set(&reg, 1, address + 2);
    Registers_set(&reg, 2, b);
    pyint expected_sum = a + b;
    pyint instruction = ADD(4, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_EQ(address, Registers_get(&reg, 1));
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_5(void)
{
    logging_info("test_PC_mode_5 relative");
    /* INC A. See p. 3-16 of LSI-11 PDP-11/03 Processor Handbook */
    pyint instruction = 0005267;
    ASSERT(ss_ops_is_ss_op(&ss, instruction));
    pyint a = 0000054, pc = 01020, address = 01100;
    Ram_write_word(&ram, pc, instruction);
    Ram_write_word(&ram, pc + 2, a);
    Ram_write_word(&ram, address, 000000);
    Registers_set_pc(&reg, pc, "test_PC_mode_5");
    Registers_set_pc(&reg, pc + 2, "test_PC_mode_5");
    ss_ops_do_ss_op(&ss, instruction);
    ASSERT_EQ(Registers_get_pc(&reg), 01024);
    ASSERT_EQ(Ram_read_word(&ram, 01100), 001);
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_6(void)
{
    logging_info("test_PC_mode_6 Relative");
    pyint instruction = ADD(6, 1, 0, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    pyint instruction_address = 02762;
    Ram_write_word(&ram, instruction_address, instruction);
    pyint X = 02050;
    Ram_write_word(&ram, instruction_address + 2, X);
    pyint operanda = 0100;
    Registers_set(&reg, 1, operanda);
    pyint operand_address = X + operanda;
    Ram_write_word(&ram, operand_address, operanda);
    pyint operandb = 02136;
    Registers_set(&reg, 2, operandb);
    pyint expected_sum = operanda + operandb;
    Registers_set_pc(&reg, instruction_address + 2, "test_mode_6");
    do_ssdd_op_report(instruction);
    ASSERT_EQ(Registers_get(&reg, 2), expected_sum);
    ASSERT_STR(nzvc(), "0000");
}

static void test_PC_mode_7(void)
{
    logging_info("test_mode_7 Relative Deferred");
    /* CLR @A. See p. 3-16 of LSI-11 PDP-11/03 Processor Handbook */
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
    RUN(test_mode_0_SD);
    RUN(test_mode_1_S);
    RUN(test_mode_1_D);
    RUN(test_mode_1_SD);
    RUN(test_mode_2_S);
    RUN(test_mode_2_D);
    RUN(test_mode_2_SD);
    RUN(test_mode_3_S);
    RUN(test_mode_4);
    RUN(test_mode_5);
    RUN(test_mode_6);
    RUN(test_mode_7);
    RUN(test_PC_mode_0);
    RUN(test_PC_mode_2);
    RUN(test_PC_mode_3);
    RUN(test_PC_mode_4);
    RUN(test_PC_mode_5);
    RUN(test_PC_mode_6);
    RUN(test_PC_mode_7);
    return TEST_SUMMARY("test_address_modes");
}
