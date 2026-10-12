/* pdp11_other_ops.c - translation of pdp11_other_ops.py (other instructions) */
#include "pdp11_other_ops.h"
#include "pdp11_logger.h"

#include <stdio.h>
#include <string.h>

/* Each instruction returns (assembly, report) */
typedef struct {
    char assembly[16];
    char report[160];
    int run;
} OtherResult;

void other_ops_init(other_ops *self, Registers *reg, Ram *ram, PSW *psw, AddressModes *am, StopWatches *sw)
{
    logging_info("initializing OtherOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->am = am;
    Stack_init(&self->stack, reg, ram, psw);
    self->sw = sw;
}

static OtherResult RTS(other_ops *self, pyint instruction)
{
    /* 00 20 0R: RTS return from subroutine 4-60
     * | PC <- reg
     * | reg <- (SP)^ */
    OtherResult o = {.run = 1};
    int R = (int)(instruction & 0000007);
    Registers_set_pc(self->reg, Registers_get(self->reg, R), "RTS");
    Registers_set(self->reg, R, Stack_pop(&self->stack));
    char a[PYSTR], b[PYSTR];
    strcpy(o.assembly, "RTS");
    snprintf(o.report, sizeof o.report, "    %s %s RTS R%d",
             py_oct(a, Registers_get_pc(self->reg)), py_oct(b, instruction), R);
    return o;
}

static OtherResult JSR(other_ops *self, pyint instruction)
{
    /* 00 4R DD: JSR jump to subroutine 4-58
     * |  pushstack(reg)
     * |  reg <- PC+2
     * |  PC <- (dst) */
    OtherResult o = {.run = 1};
    int R = (int)py_shr(instruction & 0000700, 6);
    pyint DD = instruction & 0000077;
    AMJmp j = AddressModes_addressing_mode_jmp(self->am, DD);
    if (!j.run) {
        /* FIX: JSR R,Rn is illegal; stop instead of jumping to 0 (the Python
         * pushed R and jumped to 0).  *** should trap */
        strcpy(o.assembly, "JSR");
        snprintf(o.report, sizeof o.report, "JSR with register destination is illegal");
        o.run = 0;
        return o;
    }
    /* SP is decremented before R is read, so JSR SP,dst pushes the
     * decremented SP (as SIMH and the hardware do; the Python pushed the old SP). */
    Registers_set_sp(self->reg, Registers_get_sp(self->reg) - 2, "JSR");
    Ram_write_word(self->ram, Registers_get_sp(self->reg), Registers_get(self->reg, R));
    Registers_set(self->reg, R, Registers_get_pc(self->reg));
    Registers_set_pc(self->reg, j.jump_address, "JSR");
    char a[PYSTR], b[PYSTR], c[PYSTR], d[PYSTR];
    strcpy(o.assembly, "JSR");
    snprintf(o.report, sizeof o.report, "    %s %s JSR R%d DD:%s address:%s",
             py_oct(a, Registers_get_pc(self->reg)), py_oct(b, instruction), R,
             py_oct(c, DD), py_oct(d, j.jump_address));
    return o;
}

static OtherResult unimplemented(const char *name, pyint instruction)
{
    OtherResult o = {.run = 1};
    char a[PYSTR];
    snprintf(o.assembly, sizeof o.assembly, "%s", name);
    snprintf(o.report, sizeof o.report, "%s unimplemented", py_oct(a, instruction));
    return o;
}

static OtherResult MARK(other_ops *self, pyint instruction) { (void)self; return unimplemented("MARK", instruction); } /* 00 64 NN mark 46-1 */
static OtherResult MFPI(other_ops *self, pyint instruction) { (void)self; return unimplemented("MFPI", instruction); } /* 00 65 SS 4-77 */
static OtherResult MTPI(other_ops *self, pyint instruction) { (void)self; return unimplemented("MTPI", instruction); } /* 00 66 DD 4-78 */

static const struct {
    pyint opcode;
    OtherResult (*fn)(other_ops *, pyint);
} other_instructions[] = {
    {0000200, RTS},
    {0004000, JSR},
    {0006400, MARK},
    {0006500, MFPI},
    {0006600, MTPI},
};

int other_ops_is_other_op(other_ops *self, pyint instruction)
{
    /* Using instruction bit pattern, determine whether it's an "other" instruction */
    (void)self;
    pyint masked1 = instruction & 0777700;
    pyint masked2 = instruction & 0777000;
    return masked1 == 0000200 || masked1 == 0002000 || masked1 == 0004000 || masked1 == 0006400 ||
           masked1 == 0006500 || masked1 == 0006600 || masked2 == 0004000;
}

OpResult other_ops_do_other_op(other_ops *self, pyint instruction)
{
    /* dispatch a leftover opcode */
    OpResult r = {0};
    StopWatches_start(self->sw, "other");
    pyint masked1 = instruction & 0777700;
    pyint masked2 = instruction & 0777000;
    pyint opcode = (masked2 == 0004000) ? masked2 : masked1;
    int found = 0;
    for (size_t i = 0; i < sizeof other_instructions / sizeof other_instructions[0]; i++) {
        if (other_instructions[i].opcode == opcode) {
            OtherResult o = other_instructions[i].fn(self, instruction);
            snprintf(r.assembly, sizeof r.assembly, "%s", o.assembly);
            snprintf(r.report, sizeof r.report, "%s", o.report);
            r.run = o.run;
            found = 1;
            break;
        }
    }
    if (!found) {
        char a[PYSTR];
        snprintf(r.assembly, sizeof r.assembly, "%s", py_oct(a, instruction));
        snprintf(r.report, sizeof r.report, "Error: other opcode not found");
        r.run = 0;
    }
    StopWatches_stop(self->sw, "other");
    return r;
}
