/* test_ssdd_ops.c - translation of test_ssdd_ops.py */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

#define MAX_POS_INT 0077777  /* 32767 */
#define MAX_NEG_INT 0177777  /* - 32768 */

static const pyint P = 076400;  /* 32000 */
static const pyint p = 037200;  /* 16000 */
static const pyint z = 0;
static pyint n;  /* u.twosComplementNegative(p) */
static pyint N;  /* u.twosComplementNegative(P) */

static pyint SS(int mode, int register_) { return (pyint)((mode << 3 | register_) << 6); }
static pyint DD(int mode, int register_) { return (pyint)(mode << 3 | register_); }
static pyint op(pyint opcode, int modeS, int regS, int modeD, int regD)
{
    return opcode | SS(modeS, regS) | DD(modeD, regD);
}

static void test_assert(void)
{
    ASSERT(strcmp("actual", "expected") != 0);
}

static void test_byte_mask_w(void)
{
    Logger();
    logging_info("test_byte_mask_w");
    ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'W', 0xAAAA, 0x5555), 0xAAAA);
}

static void test_byte_mask_b1(void) { ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0xFF00, 0x00FF), 0); }
static void test_byte_mask_b2(void) { ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0x00FF, 0xFF00), 0xFFFF); }
static void test_byte_mask_b3(void) { ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0xAAAA, 0x5555), 0x55AA); }
static void test_byte_mask_b4(void) { ASSERT_EQ(ssdd_ops_byte_mask(&ssdd, 'B', 0x0000, 0x5555), 0x5500); }

/* register-to-register cases: set psw and two registers, run, check */
#define RR_CASE(name, PSW0, RS, S_IN, RD, D_IN, OPCODE, ASM, D_OUT, NZVC)   \
    static void name(void)                                                  \
    {                                                                       \
        logging_info(#name);                                                \
        PSW_set_psw(&psw, .psw = (PSW0));                                   \
        Registers_set(&reg, RS, (S_IN));                                    \
        Registers_set(&reg, RD, (D_IN));                                    \
        pyint instruction = op(OPCODE, 0, RS, 0, RD);                       \
        ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));                    \
        OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);               \
        ASSERT_STR(r.assembly, ASM);                                        \
        ASSERT_EQ(Registers_get(&reg, RS), (S_IN));                         \
        ASSERT_EQ(Registers_get(&reg, RD), (D_OUT));                        \
        ASSERT_STR(nzvc(), NZVC);                                           \
    }

/*      test           psw       RS src       RD dest      opcode    assembly       result     NZVC */
RR_CASE(test_BIC_1,    0,        1, 0xAAAA,   2, 0xFFFF,   0040000, "BIC R1,R2",   0x5555,    "0000")
RR_CASE(test_BICB_1,   0,        1, 0xAAAA,   2, 0xFFFF,   0140000, "BICB R1,R2",  0xFF55,    "0000")
RR_CASE(test_BIC_2,    0777777,  3, 0001234,  4, 0001111,  0040000, "BIC R3,R4",   0000101,   "0001")
RR_CASE(test_BICB_2,   0,        3, 0001234,  4, 0001111,  0140000, "BICB R3,R4",  0001101,   "0000")
RR_CASE(test_MOV_0,    0777777,  3, 0123456,  4, 0000000,  0010000, "MOV R3,R4",   0123456,   "1001")
RR_CASE(test_MOVB_01,  0777777,  3, 0xA72E,   4, 0x0000,   0110000, "MOVB R3,R4",  0x002E,    "0001")
RR_CASE(test_MOVB_02,  0777777,  3, 0xA7AE,   4, 0x0000,   0110000, "MOVB R3,R4",  0xFFAE,    "1001") /* MOVB to a register sign-extends (Python: 0x00AE) */
RR_CASE(test_MOVB_03,  0777777,  3, 0x00AE,   4, 0xA700,   0110000, "MOVB R3,R4",  0xFFAE,    "1001") /* sign-extends (Python: 0xA7AE) */

static void test_MOVB_04(void)
{
    logging_info("test_MOVB_04");
    /* 165250 112512 MOVB (R5)+,@R2 from M9301-YA */
    PSW_set_psw(&psw, .psw = 0777777);
    Registers_set(&reg, 2, 0000500);
    Ram_write_word(&ram, 0000500, 0);
    ASSERT_EQ(Ram_read_word(&ram, 0000500), 0);

    Registers_set(&reg, 5, 0165320);
    Ram_write_word(&ram, 0165320, 0177777);
    ASSERT_EQ(Ram_read_word(&ram, Registers_get(&reg, 5)), 0177777);

    pyint instruction = 0112512;
    Registers_set_pc(&reg, 0165250, "");
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);
    ASSERT_STR(r.assembly, "MOVB (R5)+,@R2");
    ASSERT_EQ(Registers_get(&reg, 5), 0165321);
    pyint atr2 = Ram_read_byte(&ram, Registers_get(&reg, 2));
    ASSERT_EQ(atr2, 0377);
}

RR_CASE(test_ADD_PP_P,  0, 2, 486,                          4, 692,                         0060000, "ADD R2,R4", 1178,    "0000")
RR_CASE(test_ADD_PP_Z,  0, 2, 0,                            4, 0,                           0060000, "ADD R2,R4", 0,       "0100")
RR_CASE(test_ADD_PP_NVC,0, 2, MAX_POS_INT - 64,             4, MAX_POS_INT - 64,            0060000, "ADD R2,R4", 0177576, "1010") /* no carry out of bit 15 (Python: "1011") */
RR_CASE(test_ADD_NP_PC, 0, 2, twosComplementNegative(286),  4, 384,                         0060000, "ADD R2,R4", 98,      "0001")
RR_CASE(test_ADD_NP_ZC, 0, 2, twosComplementNegative(286),  4, 286,                         0060000, "ADD R2,R4", 0,       "0101")
RR_CASE(test_ADD_NP_N,  0, 2, twosComplementNegative(286),  4, 3,                           0060000, "ADD R2,R4", 65253,   "1000") /* -283 */
RR_CASE(test_ADD_NN_N,  0, 2, twosComplementNegative(286),  4, twosComplementNegative(286), 0060000, "ADD R2,R4", twosComplementNegative(572), "1001") /* carry out (Python: "1000") */
RR_CASE(test_ADD_NN_VC, 0, 2, twosComplementNegative(MAX_POS_INT - 64), 4, twosComplementNegative(MAX_POS_INT - 64), 0060000, "ADD R2,R4", 0202, "0011")

static void test_10_70(void)
{
    logging_info("test_10_70");
    /* from a situation in M9301-YA */
    PSW_set_psw(&psw, .psw = 0);
    Registers_set(&reg, 2, 0177771);
    Registers_set(&reg, 4, 010);
    pyint instruction = op(0060000, 0, 2, 0, 4);
    ssdd_ops_do_ssdd_op(&ssdd, instruction);
    ASSERT_EQ(Registers_get(&reg, 4), 001);
    ASSERT_STR(nzvc(), "0001");
}

static void test_verify_negatives(void)
{
    logging_info("test_verify_negatives");
    pyint python_n = pythonifyPDP11Word(n);
    pyint python_N = pythonifyPDP11Word(N);
    ASSERT_EQ(n, twosComplementNegative(p));
    ASSERT_EQ(N, twosComplementNegative(P));
    ASSERT(P > p);
    ASSERT(p > z);
    ASSERT(z > python_n);
    ASSERT(python_n > python_N);
}

static void test_SUB_Assembly(void)
{
    PSW_set_psw(&psw, .psw = 0);
    Registers_set(&reg, 6, N);  /* dest */
    Registers_set(&reg, 4, n);  /* source */
    pyint instruction = op(0160000, 0, 4, 0, 6);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);
    logging_info("assembly:%s", r.assembly);
    ASSERT_STR(r.assembly, "SUB R4,R6");
}

/* SUB cases: R6 = dest, R4 = source; expected result PDP11WordifyPythonInteger(...) */
#define SUB_CASE(name, DEST, SRC, EXPECTED, NZVC)                         \
    static void name(void)                                                \
    {                                                                     \
        logging_info(#name);                                              \
        PSW_set_psw(&psw, .psw = 0);                                      \
        Registers_set(&reg, 6, (DEST));                                   \
        Registers_set(&reg, 4, (SRC));                                    \
        pyint instruction = op(0160000, 0, 4, 0, 6);                      \
        ssdd_ops_do_ssdd_op(&ssdd, instruction);                          \
        ASSERT_EQ(Registers_get(&reg, 6), (EXPECTED));                    \
        ASSERT_STR(nzvc(), NZVC);                                         \
    }

#define W(x) PDP11WordifyPythonInteger(x)
SUB_CASE(test_SUB_zp_N,  z, p, W(z - p), "1001")  /* 1 zero minus small positive; borrow (Python: "1000") */
SUB_CASE(test_SUB_Pp_P,  P, p, W(P - p), "0000")  /* 2 big positive minus small positive */
SUB_CASE(test_SUB_PP_Z,  P, P, 0,        "0100")
SUB_CASE(test_SUB_pP_N,  p, P, W(p - P), "1001")  /* 4; borrow (Python: "1000") */
SUB_CASE(test_SUB_Nz_N,  N, z, W(N),     "1000")  /* 5 */
SUB_CASE(test_SUB_Np_VC, N, p, W(N - p), "0010")  /* 6; overflow, no borrow (Python: "0011") */
SUB_CASE(test_SUB_NP_V,  N, P, W(N - P), "0010")  /* 7; overflow, no borrow (Python: "0011"; defined twice there, identically) */
SUB_CASE(test_SUB_zP_N,  z, P, W(-P),    "1001")  /* 9; borrow (Python: "1000") */
SUB_CASE(test_SUB_Nn_N,  N, n, W(N - n), "1001")  /* 10; borrow (Python: "1000") */
SUB_CASE(test_SUB_NN_Z,  N, N, 0,        "0100")  /* 11 */
SUB_CASE(test_SUB_nN_P,  n, N, W(n - N), "0000")  /* 12 */
SUB_CASE(test_SUB_Zn_P,  z, n, W(-n),    "0001")  /* 13; borrow (Python: "0000") */
SUB_CASE(test_SUB_nP_V,  n, P, W(n - P), "0010")  /* 14; overflow, no borrow (Python: "0011") */
SUB_CASE(test_SUB_Pn_V,  P, N, W(P - N), "1011")  /* 16 */

int main(void)
{
    fixture_init();
    n = twosComplementNegative(p);
    N = twosComplementNegative(P);

    RUN(test_assert);
    RUN(test_byte_mask_w);
    RUN(test_byte_mask_b1);
    RUN(test_byte_mask_b2);
    RUN(test_byte_mask_b3);
    RUN(test_byte_mask_b4);
    RUN(test_BIC_1);
    RUN(test_BICB_1);
    RUN(test_BIC_2);
    RUN(test_BICB_2);
    RUN(test_MOV_0);
    RUN(test_MOVB_01);
    RUN(test_MOVB_02);
    RUN(test_MOVB_03);
    RUN(test_MOVB_04);
    RUN(test_ADD_PP_P);
    RUN(test_ADD_PP_Z);
    RUN(test_ADD_PP_NVC);
    RUN(test_ADD_NP_PC);
    RUN(test_ADD_NP_ZC);
    RUN(test_ADD_NP_N);
    RUN(test_ADD_NN_N);
    RUN(test_ADD_NN_VC);
    RUN(test_10_70);
    RUN(test_verify_negatives);
    RUN(test_SUB_Assembly);
    RUN(test_SUB_zp_N);
    RUN(test_SUB_Pp_P);
    RUN(test_SUB_PP_Z);
    RUN(test_SUB_pP_N);
    RUN(test_SUB_Nz_N);
    RUN(test_SUB_Np_VC);
    RUN(test_SUB_NP_V);
    RUN(test_SUB_zP_N);
    RUN(test_SUB_Nn_N);
    RUN(test_SUB_NN_Z);
    RUN(test_SUB_nN_P);
    RUN(test_SUB_Zn_P);
    RUN(test_SUB_nP_V);
    RUN(test_SUB_Pn_V);
    return TEST_SUMMARY("test_ssdd_ops");
}
