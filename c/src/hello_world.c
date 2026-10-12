/* hello_world.c - translation of hello_world.py
 * Loads the "Hello, World!" program into the PDP11 and runs it until it halts. */
#include <string.h>

#include "pdp11.h"
#include "pdp11_logger.h"

int main(void)
{
    static PDP11 pdp11;
    PDP11_init(&pdp11, "True");
    logging_info("hello_world begins");

    logging_info("hello_world pdp11Boot()");
    pdp11Boot boot;
    pdp11Boot_init(&boot, &pdp11.reg, &pdp11.ram);

    logging_info("hello_world load_machine_code()");
    /* http://www.retrocmp.com/how-tos/interfacing-to-a-pdp-1105/146-interfacing-with-a-pdp-1105-test-programs-and-qhello-worldq */
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
                  /* 2032         "Hello, World!" ; message */
    pyint program_address = 02000;
    pdp11Boot_load_machine_code(&boot, hello_world, (int)(sizeof hello_world / sizeof hello_world[0]), program_address);

    pyint message_address = 02032;
    const char message_text[] = "Hello, World!\nHello, World!\nHello, World!";
    for (size_t i = 0; i <= strlen(message_text); i++) {   /* includes the terminating chr(0) */
        Ram_write_byte(&pdp11.ram, message_address, (unsigned char)message_text[i]);
        message_address = message_address + 1;
    }

    Ram_dump(&pdp11.ram, program_address, message_address);

    Registers_set_pc(&pdp11.reg, 02000, "load_machine_code");
    pdp11Run run;
    pdp11Run_init(&run, &pdp11);
    pdp11Run_run(&run, 100000);
    logging_info("hello_world done");
    PDP11_free(&pdp11);
    return 0;
}
