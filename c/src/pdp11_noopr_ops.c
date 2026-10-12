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
    /* 00 00 01 Wait 4-75.  There are no interrupts to wait for, so the CPU stops.
     * FIX: the Python also advanced the PC past the next word. */
    (void)self;
    return (NooprResult){0, "WAIT: no interrupts implemented; stopping"};
}

static NooprResult RTI(noopr_ops *self)
{
    /* 00 00 02 RTI return from interrupt 4-69
     * PC < (SP)^; PS< (SP)^ */
    Registers_set_pc(self->reg, Stack_pop(self->stack), "RTI");
    PSW_set_psw(self->psw, .psw = Stack_pop(self->stack));
    return (NooprResult){1, ""};
}

static NooprResult BPT(noopr_ops *self)
{
    /* 00 00 03 BPT breakpoint trap 4-67: trap through vector 14.
     * FIX: the Python advanced the PC and stopped. */
    Stack_trap(self->stack, 014);
    return (NooprResult){1, ""};
}

static NooprResult IOT(noopr_ops *self)
{
    /* 00 00 04 IOT input/output trap 4-68
     * | push PS
     * | push PC
     * PC <- 20
     * PS <- 22
     * i.e. trap through vector 20: PC and PS are loaded from locations 20 and 22.
     * FIX: the Python pushed a nonexistent attribute (AttributeError), set PC
     * to 20 itself, and loaded 22 into SP. */
    Stack_trap(self->stack, 020);
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
    /* 00 00 06 RTT return from interrupt 4-70: PC < (SP)^; PS < (SP)^
     * (like RTI; RTT differs only in suppressing a trace trap, which is not emulated)
     * FIX: the Python popped the PS into SP and then raised AttributeError. */
    Registers_set_pc(self->reg, Stack_pop(self->stack), "RTT");
    PSW_set_psw(self->psw, .psw = Stack_pop(self->stack));
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
