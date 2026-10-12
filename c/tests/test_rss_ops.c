/* test_rss_ops.c - translation of test_rss_ops.py */
#include "fixture.h"
#include "pdp11_logger.h"
#include "test.h"

static const pyint P = 0764;  /* 500 */
static const pyint p = 0265;  /* 181 */
static const pyint p1 = 1;
static pyint n1;              /* u.twosComplementNegative(p1) */
static const pyint z = 0;
static pyint n;               /* u.twosComplementNegative(p) */
static pyint N;               /* u.twosComplementNegative(P) */

/* There are limits on what sorts of numbers can be divided without blowing things up */
static const pyint smlPos = 044276;
static const pyint bigPos = 0042400042;
static pyint smlNeg;          /* u.twosComplementNegative(smlPos) */
static pyint bigNeg;          /* u.twosComplementNegativeLong(bigPos) */

static pyint SS_part(int mode, int register_) { return (pyint)((mode << 3 | register_) << 6); }
static pyint DD_part(int mode, int register_) { return (pyint)(mode << 3 | register_); }
static pyint make_ssdd_op(pyint opcode, int modeS, int regS, int modeD, int regD)
{
    return opcode | SS_part(modeS, regS) | DD_part(modeD, regD);
}
static pyint make_rss_op(pyint opcode, int regD, int modeS, int regS)
{
    /* documentation calls second parameter SS which here uses DD_part to make */
    return opcode | (pyint)regD << 6 | DD_part(modeS, regS);
}

#define RR_CASE(name, PSW0, RS, S_IN, RD, D_IN, OPCODE, ASM, D_OUT, NZVC)   \
    static void name(void)                                                  \
    {                                                                       \
        PSW_set_psw(&psw, .psw = (PSW0));                                   \
        Registers_set(&reg, RS, (S_IN));                                    \
        Registers_set(&reg, RD, (D_IN));                                    \
        pyint instruction = make_ssdd_op(OPCODE, 0, RS, 0, RD);             \
        ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));                    \
        OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);               \
        ASSERT_STR(r.assembly, ASM);                                        \
        ASSERT_EQ(Registers_get(&reg, RS), (S_IN));                         \
        ASSERT_EQ(Registers_get(&reg, RD), (D_OUT));                        \
        ASSERT_STR(nzvc(), NZVC);                                           \
    }

RR_CASE(test_BIC,      0,       1, 0xAAAA,  2, 0xFFFF,  0040000, "BIC R1,R2",  0x5555,  "0000")
RR_CASE(test_BICB,     0,       1, 0xAAAA,  2, 0xFFFF,  0140000, "BICB R1,R2", 0xFF55,  "0000")
RR_CASE(test_BIC_2,    0777777, 3, 0001234, 4, 0001111, 0040000, "BIC R3,R4",  0000101, "0001")
RR_CASE(test_BICB_2,   0,       3, 0001234, 4, 0001111, 0140000, "BICB R3,R4", 0001101, "0000")
RR_CASE(test_MOV_0,    0777777, 3, 0123456, 4, 0000000, 0010000, "MOV R3,R4",  0123456, "1001")

static void test_MOV_2(void)
{
    /* 010322 MOV R3,(R2)+ from M9001_YA that broke */
    PSW_set_psw(&psw, .psw = 0777777);
    Registers_set(&reg, 3, 0177777);
    Registers_set(&reg, 2, 0000000);
    pyint instruction = make_ssdd_op(0010000, 0, 3, 2, 2);
    ASSERT(ssdd_ops_is_ssdd_op(&ssdd, instruction));
    OpResult r = ssdd_ops_do_ssdd_op(&ssdd, instruction);
    ASSERT_STR(r.assembly, "MOV R3,(R2)+");
    pyint r3 = Registers_get(&reg, 3);
    ASSERT_EQ(r3, 0177777);
    ASSERT_EQ(Registers_get(&reg, 2), 002);
    ASSERT_EQ(Ram_read_word(&ram, 0000000), r3);
    ASSERT_STR(nzvc(), "1001");
}

RR_CASE(test_MOVB_01,  0777777, 3, 0xA72E,  4, 0x0000,  0110000, "MOVB R3,R4", 0x002E,  "0001")
RR_CASE(test_MOVB_02,  0777777, 3, 0xA7AE,  4, 0x0000,  0110000, "MOVB R3,R4", 0x00AE,  "1001")
RR_CASE(test_MOVB_03,  0777777, 3, 0x00AE,  4, 0xA700,  0110000, "MOVB R3,R4", 0xA7AE,  "1001")

static void test_MOVB_04(void)
{
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
    ASSERT_EQ(Ram_read_byte(&ram, Registers_get(&reg, 2)), 0377);
}

/* @pytest.mark.parametrize('name, s, d, RS, RD, CC', multiplication_testcases) */
static void test_MUL(const char *name, pyint s, pyint d, int RS, int RD, const char *CC)
{
    (void)name;
    pyint instruction = 0070000;
    PSW_set_psw(&psw, .psw = 0);
    Registers_set(&reg, RD, d);
    Registers_set(&reg, RS, s);
    instruction = make_rss_op(instruction, RD, 0, RS);
    ASSERT(rss_ops_is_rss_op(&rss, instruction));
    rss_ops_do_rss_op(&rss, instruction);

    pyint expected_product = PDP11LongifyPythonInteger(pythonifyPDP11Word(s) * pythonifyPDP11Word(d));
    pyint high_word = Registers_get(&reg, RD);
    pyint low_word = Registers_get(&reg, RD + 1);
    pyint pdp11_actual_product = (high_word << 16) | low_word;
    ASSERT_EQ(pdp11_actual_product, expected_product);
    ASSERT_STR(nzvc(), CC);
}

/* @pytest.mark.parametrize('name, numerator, denominator, RS, RD, CC', division_testcases) */
static void test_DIV(const char *name, pyint numerator, pyint denominator, int RS, int RD, const char *CC)
{
    (void)name;
    /* DIV: (R, R+1) < (R, R+1) / (src) */
    pyint py_numerator = pythonifyPDP11Long(numerator);
    pyint py_denominator = pythonifyPDP11Word(denominator);
    pyint py_expected_quotient, py_expected_remainder;
    if (py_denominator == 0) {
        py_expected_quotient = 0;
        py_expected_remainder = 0;
    } else {
        py_expected_quotient = py_floordiv(py_numerator, py_denominator);
        py_expected_remainder = py_mod(py_numerator, py_denominator);
    }
    /* Fix for error in the way Python does remainder */
    if (py_numerator < 0 || py_denominator < 0) {
        py_expected_quotient = py_expected_quotient + 1;
        py_expected_remainder = py_numerator - (py_denominator * py_expected_quotient);
    }

    /* set up the environment */
    int Rvl = RD + 1;
    Registers_set(&reg, RD, numerator >> 16);         /* high-order word */
    Registers_set(&reg, Rvl, numerator & MASK_WORD);  /* low-order word */
    Registers_set(&reg, RS, denominator);

    /* set up the instruction */
    pyint instruction = 0071000;
    PSW_set_psw(&psw, .psw = 0);
    instruction = make_rss_op(instruction, RD, 0, RS);
    ASSERT(rss_ops_is_rss_op(&rss, instruction));

    /* run the instruction */
    rss_ops_do_rss_op(&rss, instruction);

    /* get the results */
    pyint py_actual_quotient = pythonifyPDP11Word(Registers_get(&reg, RD));
    pyint py_actual_remainder = pythonifyPDP11Word(Registers_get(&reg, Rvl));
    ASSERT_EQ(py_actual_quotient, py_expected_quotient);
    ASSERT_EQ(py_actual_remainder, py_expected_remainder);
    ASSERT_STR(nzvc(), CC);
}

int main(void)
{
    fixture_init();
    n1 = twosComplementNegative(p1);
    n = twosComplementNegative(p);
    N = twosComplementNegative(P);
    smlNeg = twosComplementNegative(smlPos);
    bigNeg = twosComplementNegativeLong(bigPos);

    RUN(test_BIC);
    RUN(test_BICB);
    RUN(test_BIC_2);
    RUN(test_BICB_2);
    RUN(test_MOV_0);
    RUN(test_MOV_2);
    RUN(test_MOVB_01);
    RUN(test_MOVB_02);
    RUN(test_MOVB_03);
    RUN(test_MOVB_04);

    /* 'name, s, d, RS, RD, nzvc' */
    const struct { const char *name; pyint s, d; int RS, RD; const char *CC; } multiplication_testcases[] = {
        {"pp", p, p, 2, 4, "0000"},
        {"pz", p, z, 2, 4, "0100"},
        {"pn", p, n, 2, 4, "1000"},
        {"zp", z, p, 2, 4, "0100"},
        {"zz", z, z, 2, 4, "0100"},
        {"zn", z, n, 2, 4, "0100"},
        {"p1p1", p1, p1, 2, 4, "0000"},
        {"p1n1", p1, n1, 2, 4, "1000"},
        {"n1p1", n1, p1, 2, 4, "1000"},
        {"n1n1", n1, n1, 2, 4, "0000"},
        {"np", n, p, 2, 4, "1000"},
        {"nz", n, z, 2, 4, "0100"},
        {"nn", n, n, 2, 4, "0000"},

        {"pP", p, P, 2, 4, "0000"},
        {"pN", p, N, 2, 4, "1000"},
        {"zP", z, P, 2, 4, "0100"},
        {"zN", z, N, 2, 4, "0100"},
        {"nP", n, P, 2, 4, "1000"},
        {"nN", n, N, 2, 4, "0000"},
        {"Pp", P, p, 2, 4, "0000"},
        {"Pz", P, z, 2, 4, "0100"},
        {"Pn", P, n, 2, 4, "1000"},
        {"Np", N, p, 2, 4, "1000"},
        {"Nz", N, z, 2, 4, "0100"},
        {"Nn", N, n, 2, 4, "0000"},

        {"PP", P, P, 2, 4, "0000"},
        {"PN", P, N, 2, 4, "1000"},
        {"NP", N, P, 2, 4, "1000"},
        {"NN", N, N, 2, 4, "0000"},
    };
    for (size_t i = 0; i < sizeof multiplication_testcases / sizeof multiplication_testcases[0]; i++) {
        const char *name = multiplication_testcases[i].name;
        RUN_CASE(test_MUL, name, name, multiplication_testcases[i].s, multiplication_testcases[i].d,
                 multiplication_testcases[i].RS, multiplication_testcases[i].RD, multiplication_testcases[i].CC);
    }

    /* name, numerator, denominator, RS, RD, nzvc; numerators are 32-bit pdp11 integers */
    const struct { const char *name; pyint numerator, denominator; int RS, RD; const char *CC; } division_testcases[] = {
        {"Pp", bigPos, smlPos, 2, 4, "0000"},
        {"Pn", bigPos, smlNeg, 2, 4, "1000"},
        {"Np", bigNeg, smlPos, 2, 4, "1000"},
        {"Nn", bigNeg, smlNeg, 2, 4, "0000"},
    };
    for (size_t i = 0; i < sizeof division_testcases / sizeof division_testcases[0]; i++) {
        const char *name = division_testcases[i].name;
        RUN_CASE(test_DIV, name, name, division_testcases[i].numerator, division_testcases[i].denominator,
                 division_testcases[i].RS, division_testcases[i].RD, division_testcases[i].CC);
    }
    return TEST_SUMMARY("test_rss_ops");
}
