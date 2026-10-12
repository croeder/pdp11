/* test_implemented.c - translation of test_implemented.py
 * Generate all the opcodes and log which ones are not recognized.
 * Like the Python, this has no assertions; the results go to pdp11.log. */
#include <stdlib.h>

#include "pdp11.h"
#include "pdp11_logger.h"
#include "test.h"

static void test_opcodes(void)
{
    logging_info("testing opcodes");
    static PDP11 pdp11;
    PDP11_init(&pdp11, NULL);
    pyint not_ops[01777];
    int not_count = 0;

    for (int i = 0; i < 01777; i++) {
        pyint opcode = (pyint)i << 6;
        int is_op = 0;
        const char *opcodename = "";
        const char *opcodetype = "";

        if (br_ops_is_br_op(&pdp11.br, opcode)) {
            is_op = 1;
            const char *name;
            if (br_ops_lookup(opcode, NULL, &name)) { opcodename = name; opcodetype = "BR"; }
        } else if (cc_ops_is_cc_op(&pdp11.cc_ops, opcode)) {
            is_op = 1;
            opcodename = "CC";
            opcodetype = "CC";
        } else if (noopr_ops_is_noopr_op(&pdp11.noopr_ops, opcode)) {
            is_op = 1;
            const char *name = noopr_ops_name(opcode);
            if (name) { opcodename = name; opcodetype = "NOPR"; }
        } else if (rss_ops_is_rss_op(&pdp11.rss_ops, opcode)) {
            is_op = 1;
            const char *name;
            if (rss_ops_lookup(opcode & 0077000, NULL, &name)) { opcodename = name; opcodetype = "RSS"; }
        } else if (ss_ops_is_ss_op(&pdp11.ss_ops, opcode)) {
            is_op = 1;
            const char *name;
            if (ss_ops_lookup(opcode & 0107700, NULL, &name, NULL)) { opcodename = name; opcodetype = "SS"; }
        } else if (ssdd_ops_is_ssdd_op(&pdp11.ssdd_ops, opcode)) {
            is_op = 1;
            const char *name = ssdd_ops_name(opcode & 0070000);
            if (name) { opcodename = name; opcodetype = "SSDD"; }
        } else {
            /* Python: `elif pdp11.other_ops.do_other_op(opcode & 0o777700):` - a
             * non-empty tuple, so always true; the call itself still executes. */
            other_ops_do_other_op(&pdp11.other_ops, opcode & 0777700);
            is_op = 1;
            opcodename = "other";
            opcodetype = "other";
        }

        if (!is_op) {
            not_ops[not_count++] = opcode;
            opcodename = "not";
            opcodetype = "not";
        }
        char a[PYSTR], b[PYSTR];
        logging_info("%s,%s,%s,%s", py_oct(a, opcode), py_bin(b, opcode), opcodetype, opcodename);
    }

    logging_info("found these not-opcodes:");
    logging_info("%d not-opcodes", not_count);
    pyint previous_opcode = -1;
    pyint range_begin = 0;
    int in_a_range = 0;
    for (int i = 0; i < not_count; i++) {
        pyint opcode = not_ops[i];
        char a[PYSTR], b[PYSTR];
        if (in_a_range) {
            if (opcode == previous_opcode + 1) {
                in_a_range = 1;
                previous_opcode = opcode;
            } else {
                logging_info("%s–%s", py_oct(a, range_begin), py_oct(b, previous_opcode));
                in_a_range = 0;
                previous_opcode = opcode;
            }
        } else {
            if (opcode == previous_opcode + 1) {
                in_a_range = 1;
                range_begin = previous_opcode;
                previous_opcode = opcode;
            } else {
                in_a_range = 0;
                previous_opcode = opcode;
            }
        }
    }
    if (in_a_range) {
        char a[PYSTR], b[PYSTR];
        logging_info("%s–%s", py_oct(a, range_begin), py_oct(b, previous_opcode));
    }
    PDP11_free(&pdp11);
}

int main(void)
{
    RUN(test_opcodes);
    return TEST_SUMMARY("test_implemented");
}
