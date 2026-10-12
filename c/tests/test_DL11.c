/* test_DL11.c - translation of test_DL11.py
 * (The Python version cannot run as committed: importing pdp11 pulls in
 * PySimpleGUI, and PDP11() uses threading without importing it.) */
#include <stdlib.h>
#include <time.h>

#include "pdp11.h"
#include "pdp11_logger.h"
#include "test.h"

static pyint test_character = 0;

static pyint make_test_character(void)
{
    pyint tc = 32 + rand() % (128 - 32 + 1);   /* random.randint(32,128) */
    test_character = tc;
    return tc;
}

static PDP11 pdp11;

static void test_receive(void)
{
    /* whitebox testing of receive status and buffer.
     * check status bits, stick a byte in. recheck status bits. */
    PDP11_init(&pdp11, NULL);
    logging_info("test_receive");
    DL11 *dl11 = &pdp11.dl11;

    /* verify the receive status register says nothing's ready */
    ASSERT_EQ(Ram_read_word(&pdp11.ram, dl11->RCSR_address), 00);

    /* simulate receiving a byte */
    Ram_write_word(&pdp11.ram, dl11->RBUF_address, make_test_character());

    /* verify the receiver got a byte */
    pyint RCSR = Ram_read_word(&pdp11.ram, dl11->RCSR_address);
    ASSERT_EQ(RCSR & dl11->RCSR_RCVR_DONE, dl11->RCSR_RCVR_DONE);

    /* read the buffer and verify it's what we sent */
    ASSERT_EQ(Ram_read_word(&pdp11.ram, dl11->RBUF_address), test_character);

    /* verify that the receiver is now empty */
    RCSR = Ram_read_word(&pdp11.ram, dl11->RCSR_address);
    ASSERT_EQ(RCSR & dl11->RCSR_RCVR_DONE, 00);
    logging_info("test_receive done");
}

static void test_transmit(void)
{
    /* whitebox testing of transmit status and buffer */
    PDP11_free(&pdp11);
    PDP11_init(&pdp11, NULL);
    logging_info("test_transmit");
    DL11 *dl11 = &pdp11.dl11;

    /* verify that the transmitter is ready to transmit */
    pyint XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
    ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, dl11->XCSR_XMIT_RDY);

    /* simulate transmitting a byte */
    Ram_write_word(&pdp11.ram, dl11->XBUF_address, make_test_character());

    /* verify that transmit buffer is not ready for another byte */
    XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
    ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, 0);

    /* read the buffer and verify it's what we sent */
    ASSERT_EQ(Ram_read_word(&pdp11.ram, dl11->XBUF_address), test_character);

    /* verify that the transmit buffer is ready for another byte */
    XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
    ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, dl11->XCSR_XMIT_RDY);
    logging_info("test_transmit done");
}

static void test_maintenance(void)
{
    /* set maintenance bit and verify that bytes go in and bytes go out */
    PDP11_free(&pdp11);
    PDP11_init(&pdp11, NULL);
    logging_info("test_maintenance");
    DL11 *dl11 = &pdp11.dl11;

    /* set the maintenance bit and verify it.
     * This means DL11 connects its X output to R input */
    pyint XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
    pyint set_word = XCSR | dl11->XCSR_MAINT;
    Ram_write_word(&pdp11.ram, dl11->XCSR_address, set_word);
    XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
    ASSERT_EQ(XCSR & dl11->XCSR_MAINT, dl11->XCSR_MAINT);

    /* verify that there's no byte to read */
    ASSERT_EQ(Ram_read_word(&pdp11.ram, dl11->RCSR_address), 00);

    for (int i = 0; i < 100; i++) {
        /* verify that transmitter is ready */
        XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
        ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, dl11->XCSR_XMIT_RDY);

        /* transmit a byte */
        Ram_write_word(&pdp11.ram, dl11->XBUF_address, make_test_character());

        /* verify that we've transmitted it */
        XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
        ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, 0);

        /* verify that a byte is ready to read */
        pyint RCSR = Ram_read_word(&pdp11.ram, dl11->RCSR_address);
        ASSERT_EQ(RCSR & dl11->RCSR_RCVR_DONE, dl11->RCSR_RCVR_DONE);

        /* receive the byte */
        ASSERT_EQ(Ram_read_word(&pdp11.ram, dl11->XBUF_address), test_character);

        /* verify that the transmitter is ready again */
        XCSR = Ram_read_word(&pdp11.ram, dl11->XCSR_address);
        ASSERT_EQ(XCSR & dl11->XCSR_XMIT_RDY, dl11->XCSR_XMIT_RDY);
    }
    logging_info("test_maintenance done");
}

int main(void)
{
    srand((unsigned)time(NULL));
    RUN(test_receive);
    RUN(test_transmit);
    RUN(test_maintenance);
    PDP11_free(&pdp11);
    return TEST_SUMMARY("test_DL11");
}
