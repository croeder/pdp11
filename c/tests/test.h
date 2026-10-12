/* test.h - minimal pytest-like harness for the C translation.
 *
 * As with pytest, tests run in the order they are listed, a test stops at
 * its first failing assert, and state shared through the file's "class
 * attributes" (static globals) carries from one test to the next. */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int test_failed_flag;
static int tests_passed;
static int tests_failed;

#define ASSERT(cond)                                                               \
    do {                                                                           \
        if (!(cond)) {                                                             \
            printf("    FAILED %s:%d: assert %s\n", __FILE__, __LINE__, #cond);    \
            test_failed_flag = 1;                                                  \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_EQ(actual, expected)                                                \
    do {                                                                           \
        long long a_ = (long long)(actual), e_ = (long long)(expected);            \
        if (a_ != e_) {                                                            \
            printf("    FAILED %s:%d: assert %s == %s  (%lld (0o%llo) != %lld (0o%llo))\n", \
                   __FILE__, __LINE__, #actual, #expected, a_, (unsigned long long)a_, e_, \
                   (unsigned long long)e_);                                       \
            test_failed_flag = 1;                                                  \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_STR(actual, expected)                                               \
    do {                                                                           \
        const char *a_ = (actual), *e_ = (expected);                               \
        if (strcmp(a_, e_) != 0) {                                                 \
            printf("    FAILED %s:%d: assert %s == %s  (\"%s\" != \"%s\")\n",      \
                   __FILE__, __LINE__, #actual, #expected, a_, e_);                \
            test_failed_flag = 1;                                                  \
            return;                                                                \
        }                                                                          \
    } while (0)

#define RUN(test)                                                                  \
    do {                                                                           \
        test_failed_flag = 0;                                                      \
        test();                                                                    \
        if (test_failed_flag) { tests_failed++; printf("  FAIL %s\n", #test); }    \
        else tests_passed++;                                                       \
    } while (0)

/* RUN_CASE: one case of a pytest.mark.parametrize table */
#define RUN_CASE(test, label, ...)                                                 \
    do {                                                                           \
        test_failed_flag = 0;                                                      \
        test(__VA_ARGS__);                                                         \
        if (test_failed_flag) { tests_failed++; printf("  FAIL %s[%s]\n", #test, label); } \
        else tests_passed++;                                                       \
    } while (0)

#define TEST_SUMMARY(name)                                                         \
    (printf("%-26s %d passed, %d failed\n", name, tests_passed, tests_failed), tests_failed ? 1 : 0)

#endif
