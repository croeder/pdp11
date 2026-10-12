/* test_condition_code_ops.c - translation of test_condition_code_ops.py
 * pdp11-40 ref p. 4-79 */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

static const pyint set_opcode = 0000260;
static const pyint clear_opcode = 0000240;
static const pyint psw_bits = 0000017;

static pyint CLX(pyint cvzn) { return clear_opcode | cvzn; }
static pyint SEX(pyint cvzn) { return set_opcode | cvzn; }

static void test_set_opcodes(void)
{
    for (pyint test_code = 01; test_code <= 017; test_code++) {
        PSW_set_psw(&psw, .psw = 0);
        pyint instruction = SEX(test_code);
        ASSERT(cc_ops_is_cc_op(&cc, instruction));
        OpResult r = cc_ops_do_cc_op(&cc, instruction);
        pyint result_code = PSW_get_psw(&psw) & psw_bits;
        logging_info("%s", r.assembly);
        ASSERT_EQ(test_code, result_code);
    }
}

static void test_clr_opcodes(void)
{
    for (pyint test_code = 00; test_code <= 017; test_code++) {
        PSW_set_psw(&psw, .psw = 017);
        pyint instruction = CLX(test_code);
        ASSERT(cc_ops_is_cc_op(&cc, instruction));
        pyint expected = (PSW_get_psw(&psw) & psw_bits) & ~test_code;
        OpResult r = cc_ops_do_cc_op(&cc, instruction);
        pyint result_code = PSW_get_psw(&psw) & psw_bits;
        logging_info("%s", r.assembly);
        ASSERT_EQ(expected, result_code);
    }
}

int main(void)
{
    fixture_init();
    RUN(test_set_opcodes);
    RUN(test_clr_opcodes);
    return TEST_SUMMARY("test_condition_code_ops");
}
