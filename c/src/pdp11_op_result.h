/* pdp11_op_result.h - the (run, operand1, operand2, assembly, report) tuple
 * that every do_*_op method returns in the Python. */
#ifndef PDP11_OP_RESULT_H
#define PDP11_OP_RESULT_H

#include "pdp11_util.h"

typedef struct {
    int run;
    char operand1[PYSTR];
    char operand2[PYSTR];
    char assembly[256];
    char report[160];
} OpResult;

/* (result, report) pair returned by the individual instruction methods */
typedef struct {
    pyint result;
    char report[160];
} InstrResult;

#endif
