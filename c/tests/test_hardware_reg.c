/* test_hardware_reg.c - translation of test_hardware_reg.py */
#include "fixture.h"
#include "test.h"

static void test_registers(void)
{
    for (int i = 0; i < 7; i++) {
        pyint test_value = 0123223 + i;
        Registers_set(&reg, i, test_value);
        ASSERT_EQ(Registers_get(&reg, i), test_value);
    }
}

static void test_pc(void)
{
    pyint test_value = 0165250;
    pyint expected_value = test_value;
    pyint expected_value_2 = test_value + 2;
    Registers_set_pc(&reg, test_value, "test_pc");
    ASSERT_EQ(Registers_get_pc(&reg), expected_value);
    Registers_inc_pc(&reg, "test_pc");
    ASSERT_EQ(Registers_get_pc(&reg), expected_value_2);
    Registers_set_pc(&reg, test_value, "test_pc");
    Registers_inc_pc(&reg, "test_pc");
    ASSERT_EQ(Registers_get_pc(&reg), expected_value_2);
}

static void test_set_pc_2x_offset(void)
{
    /* relies on test_pc having left PC at 0o165252 */
    pyint test_value = 0165250;
    pyint test_offset = 0123;
    pyint expected_value = test_value + 2 + test_offset * 2;
    Registers_set_pc_2x_offset(&reg, test_offset, "test_pc");
    ASSERT_EQ(Registers_get_pc(&reg), expected_value);

    test_offset = 0277;
    Registers_set_pc_2x_offset(&reg, test_offset, "test_pc");
    expected_value = 0165316;
    ASSERT_EQ(Registers_get_pc(&reg), expected_value);
}

static void test_sp(void)
{
    pyint test_value = 0123221;
    Registers_set_sp(&reg, test_value, "");
    ASSERT_EQ(Registers_get_sp(&reg), test_value);
}

int main(void)
{
    fixture_init();
    RUN(test_registers);
    RUN(test_pc);
    RUN(test_set_pc_2x_offset);
    RUN(test_sp);
    return TEST_SUMMARY("test_hardware_reg");
}
