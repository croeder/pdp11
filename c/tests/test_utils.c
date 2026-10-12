/* test_utils.c - translation of test_utils.py */
#include "pdp11_util.h"
#include "test.h"

static void test_oct3(void)
{
    char b[PYSTR];
    ASSERT_STR(oct3(b, 027), "027");
}

static void test_oct6(void)
{
    char b[PYSTR];
    ASSERT_STR(oct6(b, 02127), "002127");
}

static void test_pad(void)
{
    char b[PYSTR];
    ASSERT_STR(pad(b, "arf", 6), "arf   ");
}

static void test_twosComplementNegative(void)
{
    ASSERT_EQ(twosComplementNegative(0), 0);
    ASSERT_EQ(twosComplementNegative(1), 65535);
    ASSERT_EQ(twosComplementNegative(2), 65534);
    ASSERT_EQ(twosComplementNegative(586), 64950);
}

static void test_pythonifyPDP11Word(void)
{
    ASSERT_EQ(pythonifyPDP11Word(0), 0);
    ASSERT_EQ(pythonifyPDP11Word(1), 1);
    ASSERT_EQ(pythonifyPDP11Word(65535), -1);
    ASSERT_EQ(pythonifyPDP11Word(65534), -2);
    ASSERT_EQ(pythonifyPDP11Word(64950), -586);
}

static void test_pythonifyPDP11Byte(void)
{
    ASSERT_EQ(pythonifyPDP11Byte(0), 0);
    ASSERT_EQ(pythonifyPDP11Byte(1), 1);
    ASSERT_EQ(pythonifyPDP11Byte(255), -1);
    ASSERT_EQ(pythonifyPDP11Byte(254), -2);
    ASSERT_EQ(pythonifyPDP11Byte(127), 0177);
}

static void test_PDP11ifyPythonInt(void)
{
    ASSERT_EQ(PDP11WordifyPythonInteger(0), 0);
    ASSERT_EQ(PDP11WordifyPythonInteger(1), 1);
    ASSERT_EQ(PDP11WordifyPythonInteger(-1), 65535);
    ASSERT_EQ(PDP11WordifyPythonInteger(-2), 65534);
    ASSERT_EQ(PDP11WordifyPythonInteger(-586), 64950);
}

static void test_pythonifyPDP11Long(void)
{
    ASSERT_EQ(pythonifyPDP11Long(042400042), 9044002);
    ASSERT_EQ(pythonifyPDP11Long(037735377736LL), -9044002);
}

int main(void)
{
    RUN(test_oct3);
    RUN(test_oct6);
    RUN(test_pad);
    RUN(test_twosComplementNegative);
    RUN(test_pythonifyPDP11Word);
    RUN(test_pythonifyPDP11Byte);
    RUN(test_PDP11ifyPythonInt);
    RUN(test_pythonifyPDP11Long);
    return TEST_SUMMARY("test_utils");
}
