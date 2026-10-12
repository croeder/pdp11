/* test_ss_ops.c - translation of test_ss_ops.py */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

/* byte_mask tests (these call ssdd_ops.byte_mask, as the Python does) */

static void test_byte_mask_w(void)
{
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'W', 0xAAAA, 0x5555), 0xAAAA);
}

static void test_byte_mask_b1(void)
{
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0xFF00, 0x00FF), 0);
}

static void test_byte_mask_b2(void)
{
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0x00FF, 0xFF00), 0xFFFF);
}

static void test_byte_mask_b3(void)
{
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0xAAAA, 0x5555), 0x55AA);   /* 0b0101010110101010 */
}

static void test_byte_mask_b4(void)
{
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0x0000, 0x5555), 0x5500);   /* 0b0101010100000000 */
}

/* The single-operand tests all follow one pattern:
 *   psw.set_psw(psw=PSW0); reg.set(1, R1)
 *   assert is_ss_op(instruction); do_ss_op(instruction)
 *   assert assembly, R1, and NZVC */
#define SS_CASE(name, PSW0, R1_IN, INSTRUCTION, ASM, R1_OUT, NZVC)     \
    static void name(void)                                             \
    {                                                                  \
        logging_info("\n" #name);                                      \
        PSW_set_psw(&psw, .psw = (PSW0));                              \
        Registers_set(&reg, 1, (R1_IN));                               \
        pyint instruction = (INSTRUCTION);                             \
        ASSERT(ss_ops_is_ss_op(&ss, instruction));                     \
        OpResult r = ss_ops_do_ss_op(&ss, instruction);                \
        ASSERT_STR(r.assembly, ASM);                                   \
        ASSERT_EQ(Registers_get(&reg, 1), (R1_OUT));                   \
        ASSERT_STR(nzvc(), NZVC);                                      \
    }

/*      test           psw         R1 before  instruction  assembly    R1 after   NZVC */
SS_CASE(test_CLR,      0,          0013333,   005001,      "CLR R1",   00,        "0100")
SS_CASE(test_COM_1,    0,          0013333,   0005101,     "COM R1",   0164444,   "1001")
SS_CASE(test_COM_2,    0,          0164444,   0005101,     "COM R1",   0013333,   "0001")
SS_CASE(test_COMB_1,   0,          0000022,   0105101,     "COMB R1",  0355,      "1001")
SS_CASE(test_INC,      0,          0013333,   005201,      "INC R1",   0013334,   "0000")
SS_CASE(test_INCB,     0,          0000033,   0105201,     "INCB R1",  0000034,   "0000")
SS_CASE(test_DEC_1,    0,          0013333,   005301,      "DEC R1",   0013332,   "0000")
SS_CASE(test_DEC_2,    0,          00001,     005301,      "DEC R1",   0,         "0100")
SS_CASE(test_DEC_3,    0,          0,         005301,      "DEC R1",   0177777,   "1000") /* decrement doesn't set overflow */
SS_CASE(test_DEC_B1,   0,          0000001,   0105301,     "DECB R1",  0000000,   "0100")
SS_CASE(test_SWAB_1,   00177777,   0077777,   0000301,     "SWAB R1",  0177577,   "0000")
SS_CASE(test_SWAB_2,   00177777,   0177577,   0000301,     "SWAB R1",  0077777,   "1000")
SS_CASE(test_SWAB_3,   00177777,   0000377,   0000301,     "SWAB R1",  0177400,   "0100")
SS_CASE(test_SWAB_4,   00177777,   0177400,   0000301,     "SWAB R1",  0000377,   "1000")
SS_CASE(test_ROR_1,    00177777,   0xFF00,    0006001,     "ROR R1",   0xFF80,    "1010") /* C=1 rotates into bit 15 (Python: 0x7F80, "0000") */
SS_CASE(test_ROR_2,    00177777,   0x00FF,    0006001,     "ROR R1",   0x807F,    "1001") /* 0b1000000001111111 */
SS_CASE(test_RORB_1,   00177777,   0x0FF0,    0106001,     "RORB R1",  0x0FF8,    "1010") /* only the low byte rotates, C into bit 7 (Python: 0x8778, "0011") */
SS_CASE(test_RORB_2,   00177777,   0xF00F,    0106001,     "RORB R1",  0xF087,    "1001") /* only the low byte rotates (Python: 0x7887) */
SS_CASE(test_ROL,      00177777,   0xCC33,    0006101,     "ROL R1",   0x9867,    "1001") /* 0b1001100001100111 */
SS_CASE(test_ROLB,     00177777,   0xCC33,    0106101,     "ROLB R1",  0xCC67,    "0000") /* only the low byte rotates, C into bit 0 (Python: 0x9966, "0011") */
SS_CASE(test_ASR,      00177777,   0xCC33,    0006201,     "ASR R1",   0xE619,    "1001") /* 0b1110011000011001 */
SS_CASE(test_ASRB,     00177777,   0xCC33,    0106201,     "ASRB R1",  0xCC19,    "0011") /* only the low byte shifts (Python: 0xE619) */

int main(void)
{
    fixture_init();
    RUN(test_byte_mask_w);
    RUN(test_byte_mask_b1);
    RUN(test_byte_mask_b2);
    RUN(test_byte_mask_b3);
    RUN(test_byte_mask_b4);
    RUN(test_CLR);
    RUN(test_COM_1);
    RUN(test_COM_2);
    RUN(test_COMB_1);
    RUN(test_INC);
    RUN(test_INCB);
    RUN(test_DEC_1);
    RUN(test_DEC_2);
    RUN(test_DEC_3);
    RUN(test_DEC_B1);
    RUN(test_SWAB_1);
    RUN(test_SWAB_2);
    RUN(test_SWAB_3);
    RUN(test_SWAB_4);
    RUN(test_ROR_1);
    RUN(test_ROR_2);
    RUN(test_RORB_1);
    RUN(test_RORB_2);
    RUN(test_ROL);
    RUN(test_ROLB);
    RUN(test_ASR);
    RUN(test_ASRB);
    return TEST_SUMMARY("test_ss_ops");
}
