/* pdp11_noopr_ops.c - translation of pdp11_noopr_ops.py
 * no-operand instructions 00 00 00 through 00 00 06 */
#include "pdp11_noopr_ops.h"
#include "pdp11_logger.h"

#include <stdio.h>
#include <string.h>

/* Each instruction returns (run, report) */
typedef struct {
    int run;
    const char *report;
} NooprResult;

void noopr_ops_init(noopr_ops *self, Registers *reg, Ram *ram, PSW *psw, Stack *stack, StopWatches *sw)
{
    logging_info("initializing NoOperandOps");
    self->reg = reg;
    self->ram = ram;
    self->psw = psw;
    self->stack = stack;
    self->sw = sw;
}

static NooprResult HALT(noopr_ops *self)
{
    /* 00 00 00 Halt */
    (void)self;
    return (NooprResult){0, ""};
}

static NooprResult WAIT(noopr_ops *self)
{
    /* 00 00 01 Wait 4-75   *** unimplemented */
    Registers_inc_pc(self->reg, "WAIT");
    return (NooprResult){0, "WAIT unimplemented"};
}

static NooprResult RTI(noopr_ops *self)
{
    /* 00 00 02 RTI return from interrupt 4-69
     * PC < (SP)^; PS< (SP)^ */
    Registers_set_pc(self->reg, Stack_pop(self->stack), "RTI");
    PSW_set_psw(self->psw, .psw = Stack_pop(self->stack));
    return (NooprResult){1, "RTI unimplemented"};
}

static NooprResult BPT(noopr_ops *self)
{
    /* 00 00 03 BPT breakpoint trap 4-67   *** unimplemented */
    Registers_inc_pc(self->reg, "BPT");
    return (NooprResult){0, "BPT unimplemented"};
}

static NooprResult IOT(noopr_ops *self)
{
    /* 00 00 04 IOT input/output trap 4-68
     * | push PS
     * | push PC
     * PC <- 20
     * PS <- 22
     * TRANSLATION NOTE: the Python pushes self.psw.PSW, an attribute that
     * does not exist (AttributeError).  The C pushes the PSW value, which is
     * what the comment describes.  The rest is as written, including loading
     * 0o22 into SP rather than PS. */
    Stack_push(self->stack, self->psw->psw);
    Stack_push(self->stack, Registers_get_pc(self->reg));
    Registers_set_pc(self->reg, 020, "IOT");
    Registers_set_sp(self->reg, 022, "IOT");
    return (NooprResult){1, ""};
}

static NooprResult RESET(noopr_ops *self)
{
    /* 00 00 05 RESET reset external bus 4-76 */
    (void)self;
    return (NooprResult){1, "; RESET unimplemented (mostly harmless)"};
}

static NooprResult RTT(noopr_ops *self)
{
    /* 00 00 06 RTT return from interrupt 4-70
     * TRANSLATION NOTE: the Python ends with
     * self.psw.set_condition_codes('W', self.reg.get_sp(), "***"), a method
     * PSW does not have (AttributeError).  That call is left out here. */
    Registers_set_pc(self->reg, Stack_pop(self->stack), "RTT");
    Registers_set_sp(self->reg, Stack_pop(self->stack), "RTT");
    return (NooprResult){1, ""};
}

static const struct {
    pyint opcode;
    NooprResult (*fn)(noopr_ops *);
    const char *name;
} no_operand_instructions[] = {
    {0000000, HALT, "HALT"},
    {0000001, WAIT, "WAIT"},
    {0000002, RTI, "RTI"},
    {0000003, BPT, "BPT"},
    {0000004, IOT, "IOT"},
    {0000005, RESET, "RESET"},
    {0000006, RTT, "RTT"},
};

static int find(pyint instruction)
{
    for (int i = 0; i < (int)(sizeof no_operand_instructions / sizeof no_operand_instructions[0]); i++)
        if (no_operand_instructions[i].opcode == instruction) return i;
    return -1;
}

const char *noopr_ops_name(pyint instruction)
{
    int i = find(instruction);
    return i < 0 ? NULL : no_operand_instructions[i].name;
}

int noopr_ops_is_noopr_op(noopr_ops *self, pyint instruction)
{
    /* Using instruction bit pattern, determine whether it's a no-operand instruction */
    (void)self;
    return find(instruction) >= 0;
}

OpResult noopr_ops_do_noopr_op(noopr_ops *self, pyint instruction)
{
    /* dispatch a no-operand opcode of form * 000 0** *** *** *** */
    StopWatches_start(self->sw, "noopr");
    OpResult r = {0};
    int i = find(instruction);
    if (i >= 0) {
        snprintf(r.assembly, sizeof r.assembly, "%s", no_operand_instructions[i].name);
        NooprResult nr = no_operand_instructions[i].fn(self);
        r.run = nr.run;
        snprintf(r.report, sizeof r.report, "%s", nr.report);
    } else {
        r.assembly[0] = '\0';
        snprintf(r.report, sizeof r.report, "Error: no-operand opcode not found");
        r.run = 0;
    }
    StopWatches_stop(self->sw, "noopr");
    return r;
}
