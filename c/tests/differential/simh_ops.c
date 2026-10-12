/* simh_ops.c - C side of the comparison with SIMH (see simh_check.py).
 *
 * Reads one case per line: R0..R7, PSW, then (address value) pairs to
 * deposit.  Each case runs in a child process, so an emulator stop on an
 * assertion is reported as CRASH rather than ending the run.  Prints one
 * result line per case: R0..R7, PSW, an I/O-page-touched flag, then the
 * words of the compared memory regions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fixture.h"

static const pyint regions[][2] = {{0400, 0500}, {01000, 01200}};

/* Any access to the I/O page (other than the PSW) is flagged, because SIMH
 * has real device registers there and the fixture has none. */
static int touched_io = 0;
static pyint io_read(void *device) { (void)device; touched_io = 1; return 0; }
static void io_write(void *device, pyint data) { (void)device; (void)data; touched_io = 1; }

static void run_case(const char *line)
{
    fixture_init();
    memset(ram.memory, 0, (size_t)ram.io_space);
    for (pyint a = ram.io_space; a < psw.psw_address; a++) {
        Ram_register_io_reader(&ram, a, NULL, io_read, "io_read");
        Ram_register_io_writer(&ram, a, NULL, io_write, "io_write");
    }

    long long regs[8], start_psw;
    int used = 0;
    const char *p = line;
    for (int i = 0; i < 8; i++) { sscanf(p, "%lld%n", &regs[i], &used); p += used; }
    sscanf(p, "%lld%n", &start_psw, &used); p += used;
    long long address, value;
    while (sscanf(p, "%llo %llo%n", &address, &value, &used) == 2) {
        p += used;
        Ram_write_word(&ram, address, value);
    }
    for (int i = 0; i < 8; i++) Registers_set(&reg, i, regs[i]);
    PSW_set_psw(&psw, .psw = start_psw);

    /* one fetch-decode-execute cycle, dispatched as in PDP11_dispatch_opcode */
    pyint instruction = Ram_read_word_from_pc(&ram);
    if (cc_ops_is_cc_op(&cc, instruction)) cc_ops_do_cc_op(&cc, instruction);
    else if (br_ops_is_br_op(&br, instruction)) br_ops_do_br_op(&br, instruction);
    else if (noopr_ops_is_noopr_op(&noopr, instruction)) noopr_ops_do_noopr_op(&noopr, instruction);
    else if (ss_ops_is_ss_op(&ss, instruction)) ss_ops_do_ss_op(&ss, instruction);
    else if (rss_ops_is_rss_op(&rss, instruction)) rss_ops_do_rss_op(&rss, instruction);
    else if (ssdd_ops_is_ssdd_op(&ssdd, instruction)) ssdd_ops_do_ssdd_op(&ssdd, instruction);
    else other_ops_do_other_op(&other, instruction);

    for (int i = 0; i < 8; i++) printf("%06llo ", (unsigned long long)Registers_get(&reg, i));
    printf("%06llo %d", (unsigned long long)(psw.psw & MASK_WORD), touched_io);
    for (size_t r = 0; r < sizeof regions / sizeof regions[0]; r++)
        for (pyint a = regions[r][0]; a < regions[r][1]; a += 2)
            printf(" %06llo", (unsigned long long)Ram_read_word(&ram, a));
    printf("\n");
}

int main(void)
{
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        fflush(stdout);
        int fds[2];
        if (pipe(fds) != 0) return 2;
        pid_t pid = fork();
        if (pid == 0) {
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
            dup2(fds[1], STDERR_FILENO);
            run_case(line);
            fflush(stdout);
            _exit(0);
        }
        close(fds[1]);
        char out[8192];
        size_t n = 0;
        ssize_t got;
        while ((got = read(fds[0], out + n, sizeof out - 1 - n)) > 0) n += (size_t)got;
        out[n] = '\0';
        close(fds[0]);
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            fputs(out, stdout);
        } else {
            for (char *c = out; *c; c++) if (*c == '\n') *c = ' ';
            printf("CRASH %s\n", out);
        }
    }
    return 0;
}
