/* diff_ops.c - differential test, C side.
 * Replays the cases written by gen_cases.py and compares the C translation's
 * results (registers, PSW, run flag, disassembly, memory checksum) with the
 * Python's.
 * Usage: diff_ops < cases.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture.h"

#define MEMORY_TOP 0160000
#define INSTRUCTION_ADDRESS 0400

static void lcg_fill(Ram *r, long long seed)
{
    unsigned long long x = (unsigned long long)seed & 0x7fffffff;
    for (pyint address = 0; address < MEMORY_TOP; address++) {
        x = (x * 1103515245ULL + 12345ULL) & 0x7fffffff;
        r->memory[address] = (uint8_t)((x >> 16) & 0xff);
    }
}

static unsigned long long checksum(Ram *r)
{
    unsigned long long total = 0;
    for (pyint address = 0; address < MEMORY_TOP; address++)
        total = (total * 31 + r->memory[address] + (unsigned long long)address) & 0xffffffffffffULL;
    return total;
}

int main(void)
{
    char line[1024];
    int cases = 0, mismatches = 0;
    while (fgets(line, sizeof line, stdin)) {
        long long memseed, instruction, start_psw, regs[8], run, after[8], end_psw;
        unsigned long long mem;
        char assembly[300];
        int n = sscanf(line,
                       "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld "
                       "%lld %lld %lld %lld %lld %lld %lld %lld %lld %llu %299s",
                       &memseed, &instruction, &start_psw, &regs[0], &regs[1], &regs[2], &regs[3],
                       &regs[4], &regs[5], &regs[6], &regs[7], &run, &after[0], &after[1], &after[2],
                       &after[3], &after[4], &after[5], &after[6], &after[7], &end_psw, &mem, assembly);
        if (n != 23) continue;
        for (char *c = assembly; *c; c++) if (*c == '_') *c = ' ';
        if (!strcmp(assembly, "-")) assembly[0] = '\0';

        fixture_init();
        lcg_fill(&ram, memseed);
        Ram_write_word(&ram, INSTRUCTION_ADDRESS, instruction);
        for (int i = 0; i < 8; i++) Registers_set(&reg, i, regs[i]);
        PSW_set_psw(&psw, .psw = start_psw);

        OpResult r;
        if (cc_ops_is_cc_op(&cc, instruction)) r = cc_ops_do_cc_op(&cc, instruction);
        else if (br_ops_is_br_op(&br, instruction)) r = br_ops_do_br_op(&br, instruction);
        else if (noopr_ops_is_noopr_op(&noopr, instruction)) r = noopr_ops_do_noopr_op(&noopr, instruction);
        else if (ss_ops_is_ss_op(&ss, instruction)) r = ss_ops_do_ss_op(&ss, instruction);
        else if (rss_ops_is_rss_op(&rss, instruction)) r = rss_ops_do_rss_op(&rss, instruction);
        else if (ssdd_ops_is_ssdd_op(&ssdd, instruction)) r = ssdd_ops_do_ssdd_op(&ssdd, instruction);
        else r = other_ops_do_other_op(&other, instruction);

        int bad = 0;
        if ((r.run != 0) != (run != 0)) bad = 1;
        if (strcmp(r.assembly, assembly)) bad = 1;
        for (int i = 0; i < 8; i++) if (Registers_get(&reg, i) != after[i]) bad = 1;
        if (psw.psw != end_psw) bad = 1;
        unsigned long long c_mem = checksum(&ram);
        if (c_mem != mem) bad = 1;

        if (bad) {
            mismatches++;
            if (mismatches <= 20) {
                printf("MISMATCH instruction %06llo (%s) psw %06llo\n", (unsigned long long)instruction,
                       assembly, (unsigned long long)start_psw);
                printf("  python: run=%lld psw=%06llo mem=%llx regs", run, (unsigned long long)end_psw, mem);
                for (int i = 0; i < 8; i++) printf(" %lld", after[i]);
                printf("\n  c:      run=%d psw=%06llo mem=%llx regs", r.run, (unsigned long long)psw.psw, c_mem);
                for (int i = 0; i < 8; i++) printf(" %lld", (long long)Registers_get(&reg, i));
                printf("  asm=\"%s\"\n", r.assembly);
            }
        }
        Ram_free(&ram);
        StopWatches_reset(&sw);
        cases++;
    }
    printf("%d cases, %d mismatches\n", cases, mismatches);
    return mismatches ? 1 : 0;
}
