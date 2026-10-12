/* test_hardware_psw.c - translation of test_hardware_psw.py */
#include "fixture.h"
#include "test.h"

static pyint add_byte(PSW *p, pyint b1, pyint b2)
{
    /* add byte, limit to 8 bits, set PSW */
    pyint result = b1 + b2;
    if (result > (result & MASK_LOW_BYTE)) PSW_set_psw(p, .v = 1);
    if (result == 0) PSW_set_psw(p, .z = 1);
    return result & MASK_LOW_BYTE;
}

static pyint subtract_byte(PSW *p, pyint b1, pyint b2)
{
    /* subtract bytes b1 - b2, limit to 8 bits, set PSW */
    pyint result = b1 - b2;
    if (result < 0) PSW_set_psw(p, .n = 1);
    return result & MASK_LOW_BYTE;
}

static pyint add_word(PSW *p, pyint b1, pyint b2)
{
    /* add words, limit to 16 bits, set PSW */
    pyint result = b1 + b2;
    if (result > (result & MASK_WORD)) PSW_set_psw(p, .v = 1);
    if (result == 0) PSW_set_psw(p, .z = 1);
    return result & MASK_WORD;
}

static pyint subtract_word(PSW *p, pyint b1, pyint b2)
{
    /* subtract words b1 - b2, limit to 16 bits, set PSW */
    pyint result = b1 - b2;
    if (result < 0) PSW_set_psw(p, .n = 1);
    return result & MASK_WORD;
}

static void test_psw_address(void)
{
    ASSERT_EQ(psw.psw_address, ram.top_of_memory - 1);
}

static void test_psw_set(void)
{
    pyint test_value = 0123236;
    PSW_set_psw(&psw, .psw = test_value);
    ASSERT_EQ(psw.psw, test_value);
}

static void test_psw_mode(void)
{
    PSW_set_psw(&psw, .mode = 0);
    PSW_set_psw(&psw, .mode = 3);
    ASSERT_EQ(psw.psw & 0140000, 0140000);
}

static void test_psw_priority(void)
{
    PSW_set_psw(&psw, .priority = 0);
    PSW_set_psw(&psw, .priority = 15);
    ASSERT_EQ(psw.psw & 0000340, 0000340);
}

static void test_psw_trap(void)
{
    PSW_set_psw(&psw, .trap = 0);
    PSW_set_psw(&psw, .trap = 1);
    ASSERT_EQ(psw.psw & 0000020, 0000020);
}

static void test_psw_N(void)
{
    PSW_set_psw(&psw, .n = 0);
    PSW_set_psw(&psw, .n = 1);
    ASSERT_EQ(psw.psw & 0000010, 0000010);
}

static void test_psw_Z(void)
{
    PSW_set_psw(&psw, .z = 0);
    PSW_set_psw(&psw, .z = 1);
    ASSERT_EQ(psw.psw & 0000004, 0000004);
}

static void test_psw_V(void)
{
    PSW_set_psw(&psw, .v = 0);
    PSW_set_psw(&psw, .v = 1);
    ASSERT_EQ(psw.psw & 0000002, 0000002);
}

static void test_psw_C(void)
{
    PSW_set_psw(&psw, .c = 0);
    PSW_set_psw(&psw, .c = 1);
    ASSERT_EQ(psw.psw & 0000001, 0000001);
}

static void test_psw_addb(void)
{
    PSW_set_psw(&psw, .psw = 0);
    pyint a = 3, b = 4;
    pyint sum = add_byte(&psw, a, b);
    ASSERT_EQ(sum, a + b);
    ASSERT_EQ(PSW_get_v(&psw), 0);
    ASSERT_EQ(PSW_get_z(&psw), 0);
}

static void test_psw_addb_v(void)
{
    PSW_set_psw(&psw, .psw = 0);
    pyint a = 191, b = 191;
    pyint sum = add_byte(&psw, a, b);
    ASSERT_EQ(sum, 126);
    ASSERT_EQ(PSW_get_v(&psw), 1);
    ASSERT_EQ(PSW_get_z(&psw), 0);
}

int main(void)
{
    fixture_init();
    (void)subtract_byte; (void)add_word; (void)subtract_word;  /* defined but unused in the Python too */
    RUN(test_psw_address);
    RUN(test_psw_set);
    RUN(test_psw_mode);
    RUN(test_psw_priority);
    RUN(test_psw_trap);
    RUN(test_psw_N);
    RUN(test_psw_Z);
    RUN(test_psw_V);
    RUN(test_psw_C);
    RUN(test_psw_addb);
    RUN(test_psw_addb_v);
    return TEST_SUMMARY("test_hardware_psw");
}
