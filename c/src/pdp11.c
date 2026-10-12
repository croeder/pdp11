/* pdp11.c - translation of pdp11.py (PDP-11 Emulator)
 *
 * TRANSLATION NOTES
 * - The Python PDP11.__init__ uses threading without importing it, and
 *   instruction_cycle refers to self.pdp11.CPU_cycles, which does not exist,
 *   so the whole-machine path does not run as committed.  The C does what
 *   that code evidently intends (CPU_cycles of this object).
 * - The CPU thread, VT52 and Tek 4010 consoles are PySimpleGUI/tkinter code
 *   and are not translated.  pdp11Run_run() is the Python run() loop with
 *   the stdio Terminal; run() called terminal.window_cycle(), which Terminal
 *   does not define, so the C calls Terminal_cycle().
 * - instruction_cycle logs an error when the PC did not change but, as in
 *   the Python, does not halt (it sets an unused variable there). */
#include "pdp11.h"
#include "pdp11_logger.h"

#include <stdlib.h>
#include <string.h>

void PDP11_init(PDP11 *self, const char *ui)
{
    /* instantiate the PDP11 emulator components */
    Logger();
    logging_info("pdp11CPU initializing with ui=%s", ui ? ui : "None");
    StopWatches_init(&self->sw);
    Lock_init(&self->lock);

    Config config;
    Config_init(&config, NULL);
    ConfigValue bits = Config_lookup(&config, "ram", "bits");

    /* hardware */
    Registers_init(&self->reg);
    Ram_init(&self->ram, &self->lock, &self->reg, ConfigValue_int(&bits));
    PSW_init(&self->psw, &self->ram);
    Stack_init(&self->stack, &self->reg, &self->ram, &self->psw);
    AddressModes_init(&self->am, &self->reg, &self->ram, &self->psw);

    /* operations */
    br_ops_init(&self->br, &self->reg, &self->ram, &self->psw, &self->sw);
    noopr_ops_init(&self->noopr_ops, &self->reg, &self->ram, &self->psw, &self->stack, &self->sw);
    ss_ops_init(&self->ss_ops, &self->reg, &self->ram, &self->psw, &self->am, &self->sw);
    ssdd_ops_init(&self->ssdd_ops, &self->reg, &self->ram, &self->psw, &self->am, &self->sw);
    rss_ops_init(&self->rss_ops, &self->reg, &self->ram, &self->psw, &self->am, &self->sw);
    other_ops_init(&self->other_ops, &self->reg, &self->ram, &self->psw, &self->am, &self->sw);
    cc_ops_init(&self->cc_ops, &self->psw, &self->sw);

    self->executed = calloc(0200000, sizeof *self->executed);
    self->executed_order = calloc(0200000, sizeof *self->executed_order);
    self->executed_count = 0;
    self->CPU_cycles = 0;

    self->run = 0;

    /* i/o devices */
    pdp11Boot_init(&self->boot, &self->reg, &self->ram);
    M9301_init(&self->m9301, &self->reg, &self->ram, &self->boot);

    /* set up DL11: reader status register 177560 */
    logging_info("pdp11CPU setting up DL111 at 0o177560");
    DL11_init(&self->dl11, &self->ram, 0177560);
    logging_info("pdp11CPU initializing ui:%s", ui ? ui : "None");
    Terminal_init(&self->terminal, &self->dl11, &self->sw);

    logging_info("pdp11CPU initializing done");
}

void PDP11_free(PDP11 *self)
{
    for (int i = 0; i < 0200000; i++) free(self->executed[i]);
    free(self->executed);
    free(self->executed_order);
    Ram_free(&self->ram);
    StopWatches_reset(&self->sw);
}

void PDP11_set_run(PDP11 *self, int new_run)
{
    /* run setter (thread-safe in the Python; single-threaded here) */
    if (self->run != new_run) {
        self->run = new_run;
        logging_info("set_run(%s)", self->run ? "True" : "False");
    }
}

int PDP11_get_run(PDP11 *self)
{
    return self->run;
}

OpResult PDP11_dispatch_opcode(PDP11 *self, pyint instruction)
{
    /* top-level dispatch
     * *** Redo this based on the table in PDP-11-10 processor manual.pdf II-1-34 */
    if (cc_ops_is_cc_op(&self->cc_ops, instruction))
        return cc_ops_do_cc_op(&self->cc_ops, instruction);
    if (br_ops_is_br_op(&self->br, instruction))
        return br_ops_do_br_op(&self->br, instruction);
    if (noopr_ops_is_noopr_op(&self->noopr_ops, instruction))
        return noopr_ops_do_noopr_op(&self->noopr_ops, instruction);
    if (ss_ops_is_ss_op(&self->ss_ops, instruction))
        return ss_ops_do_ss_op(&self->ss_ops, instruction);
    if (rss_ops_is_rss_op(&self->rss_ops, instruction))
        return rss_ops_do_rss_op(&self->rss_ops, instruction);
    if (ssdd_ops_is_ssdd_op(&self->ssdd_ops, instruction))
        return ssdd_ops_do_ssdd_op(&self->ssdd_ops, instruction);
    return other_ops_do_other_op(&self->other_ops, instruction);
}

int PDP11_instruction_cycle(PDP11 *self)
{
    /* Run one PDP11 fetch-decode-execute cycle */
    StopWatches_start(&self->sw, "instruction_cycle");
    pyint pc = Registers_get_pc(&self->reg);                  /* get pc without incrementing */
    pyint instruction = Ram_read_word_from_pc(&self->ram);    /* read at pc and increment pc */
    OpResult r = PDP11_dispatch_opcode(self, instruction);
    logging_debug("instruction_cycle came back from dispatch_opcode");
    if (log_enabled(LOG_DEBUG)) {
        char a[PYSTR], b[PYSTR], c[PYSTR + 160], regs[160], nzvc[8];
        logging_debug("instruction_cycle: %lld %s %s %s;%s NZVC:%s", (long long)self->CPU_cycles,
                      oct6(a, pc), oct6(b, instruction), pad(c, r.assembly, 20),
                      Registers_registers_to_string(&self->reg, regs, sizeof regs), PSW_get_nzvc(&self->psw, nzvc));
    }
    if (pc == Registers_get_pc(&self->reg)) {
        char a[PYSTR];
        logging_error("instruction_cycle: pc was not changed at %s. Halting.", py_oct(a, pc));
    }
    StopWatches_stop(&self->sw, "instruction_cycle");

    /* self.executed[instruction] = f'{instruction},{assembly}' */
    int key = (int)(instruction & 0177777);
    char entry[PYSTR + 160];
    snprintf(entry, sizeof entry, "%lld,%s", (long long)instruction, r.assembly);
    if (!self->executed[key])
        self->executed_order[self->executed_count++] = key;
    free(self->executed[key]);
    self->executed[key] = strdup(entry);

    self->CPU_cycles = self->CPU_cycles + 1;
    return r.run;
}

void pdp11Run_init(pdp11Run *self, PDP11 *pdp11)
{
    /* sets up and runs PDP11 emulator */
    logging_info("initializing PDP11 emulator");
    self->pdp11 = pdp11;
}

void pdp11Run_run(pdp11Run *self, pyint limit)
{
    PDP11 *pdp11 = self->pdp11;
    char regs[160], nzvc[8];
    logging_info("run: begin PDP11 emulator");
    logging_info("%s NZVC:%s", Registers_registers_to_string(&pdp11->reg, regs, sizeof regs), PSW_get_nzvc(&pdp11->psw, nzvc));

    /* start the processor loop */
    PDP11_set_run(pdp11, 1);
    pdp11->CPU_cycles = 0;
    StopWatches_start(&pdp11->sw, "CPU");
    while (PDP11_get_run(pdp11)) {
        PDP11_set_run(pdp11, PDP11_instruction_cycle(pdp11));
        pdp11->CPU_cycles = pdp11->CPU_cycles + 1;   /* counted twice, as in the Python */
        Terminal_cycle(&pdp11->terminal);
        if (limit >= 0 && pdp11->CPU_cycles > limit) {
            logging_info("run: instruction limit reached");
            PDP11_set_run(pdp11, 0);
        }
    }
    StopWatches_stop(&pdp11->sw, "CPU");

    logging_info("run: stop PDP11 emulator");
    AddressModes_address_mode_report(&pdp11->am);
    StopWatches_report(&pdp11->sw);

    double run_time = (double)StopWatches_get_watch(&pdp11->sw, "CPU")->sum / 1000000000.0;
    logging_info("run_time:%g", run_time);
    double processor_speed = run_time > 0 ? (double)pdp11->CPU_cycles / run_time : 0;  /* (cycles per second) */
    logging_info("self.pdp11.CPU_cycles:%lld", (long long)pdp11->CPU_cycles);
    logging_info("processor speed: %lld cycles / %g seconds = %5.0f instructions per second",
                 (long long)pdp11->CPU_cycles, run_time, processor_speed);
    logging_info("instructions executed:");
    for (int i = 0; i < pdp11->executed_count; i++)
        logging_info("%s", pdp11->executed[pdp11->executed_order[i]]);
    logging_info("instructions executed report ends");
}
