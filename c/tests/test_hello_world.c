/* test_hello_world.c - translation of test_hello_world.py
 *
 * The Python test loads the program and opens the VT52 window, with no
 * assertion.  Here the program runs on the stdio terminal with its output
 * captured, and the test checks that the message came out and the CPU halted. */
#include <stdlib.h>
#include <string.h>

#include "pdp11.h"
#include "pdp11_logger.h"
#include "test.h"

static void test_hello_world(void)
{
    static PDP11 pdp11;
    PDP11_init(&pdp11, "True");
    logging_info("test_hello_world begins");

    logging_info("test_hello_world pdp11Boot()");
    pdp11Boot boot;
    pdp11Boot_init(&boot, &pdp11.reg, &pdp11.ram);

    logging_info("test_hello_world load_machine_code()");
    const pyint hello_world[] = {
        0012702,  /* 2000 start:  MOV */
        0177564,  /* 2002         serial+4,r2    ; r2 points to DL11 */
        0012701,  /* 2004         MOV */
        0002032,  /* 2006         string,r1      ; r1 points to current char */
        0112100,  /* 2010 nxtchr: MOVB (r1)+,r0  ; load xmt char */
        0001405,  /* 2012         BEQ done       ; string is terminated with 0 */
        0110062,  /* 2014         MOVB r0,2(r2)  ; write char to transmit buffer */
        0000002,  /* 2016 */
        0105712,  /* 2020 wait:   TSTB (r2)      ; character transmitted? */
        0100376,  /* 2022         BPL wait       ; no, loop */
        0000771,  /* 2024         BR nxtchr      ; transmit next character */
        0000000,  /* 2026         halt */
        0000763}; /* 2030         br start */
    pyint program_address = 02000;
    pdp11Boot_load_machine_code(&boot, hello_world, (int)(sizeof hello_world / sizeof hello_world[0]), program_address);

    pyint message_address = 02032;
    const char message_text[] = "Hello, World!\nHello, World!\nHello, World!";
    for (size_t i = 0; i <= strlen(message_text); i++) {
        Ram_write_byte(&pdp11.ram, message_address, (unsigned char)message_text[i]);
        message_address = message_address + 1;
    }
    Ram_dump(&pdp11.ram, program_address, message_address);
    Registers_set_pc(&pdp11.reg, 02000, "load_machine_code");

    /* capture the terminal's output; no keyboard input */
    char *captured = NULL;
    size_t captured_len = 0;
    pdp11.terminal.out = open_memstream(&captured, &captured_len);
    pdp11.terminal.input_fd = -1;

    pdp11Run run;
    pdp11Run_init(&run, &pdp11);
    pdp11Run_run(&run, 100000);
    fclose(pdp11.terminal.out);

    ASSERT_STR(captured, message_text);
    ASSERT_EQ(Registers_get_pc(&pdp11.reg), 02030);   /* stopped just past the HALT at 2026 */
    ASSERT(pdp11.CPU_cycles < 100000);                 /* halted, not stopped by the limit */
    free(captured);
    PDP11_free(&pdp11);
    logging_info("test_hello_world done");
}

int main(void)
{
    RUN(test_hello_world);
    return TEST_SUMMARY("test_hello_world");
}
