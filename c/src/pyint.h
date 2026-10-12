/* pyint.h - helpers that reproduce Python integer semantics in C.
 *
 * The Python emulator relies on Python's unbounded, signed integers:
 * values can go negative, can exceed 16 bits, and the bit operators act on
 * an infinite two's-complement representation.  Every "Python int" in this
 * translation is an int64_t (pyint).  For the value ranges this emulator
 * uses, int64_t two's complement gives the same results as Python for
 * & | ^ ~ + - *.  The helpers below cover the operators whose C behaviour
 * differs from Python's: <<, >>, // and %.
 */
#ifndef PYINT_H
#define PYINT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int64_t pyint;

/* Python raises an exception here; the C translation stops the program. */
#define PY_RAISE(kind, msg) \
    do { fprintf(stderr, "%s: %s (%s:%d)\n", kind, msg, __FILE__, __LINE__); abort(); } while (0)

/* a << n with Python semantics (no undefined behaviour on negatives) */
static inline pyint py_shl(pyint a, pyint n)
{
    if (n < 0) PY_RAISE("ValueError", "negative shift count");
    if (n >= 63) {
        if (a == 0) return 0;
        PY_RAISE("OverflowError", "left shift exceeds 64-bit translation of a Python int");
    }
    return (pyint)((uint64_t)a << n);
}

/* a >> n with Python semantics (arithmetic shift, any n >= 0) */
static inline pyint py_shr(pyint a, pyint n)
{
    if (n < 0) PY_RAISE("ValueError", "negative shift count");
    if (n >= 63) return a < 0 ? -1 : 0;
    return a >> n;   /* gcc/clang: arithmetic shift for signed values */
}

/* a // b (floor division) */
static inline pyint py_floordiv(pyint a, pyint b)
{
    if (b == 0) PY_RAISE("ZeroDivisionError", "integer division or modulo by zero");
    pyint q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q -= 1;
    return q;
}

/* a % b (result has the sign of b) */
static inline pyint py_mod(pyint a, pyint b)
{
    if (b == 0) PY_RAISE("ZeroDivisionError", "integer division or modulo by zero");
    pyint r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

static inline pyint py_abs(pyint a) { return a < 0 ? -a : a; }

#endif
