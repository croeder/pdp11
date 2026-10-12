/* pdp11_terminal.c - translation of pdp11_terminal.py, as a stdin/stdout terminal. */
#include "pdp11_terminal.h"
#include "pdp11_logger.h"

#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void Terminal_init(Terminal *self, DL11 *dl11, StopWatches *sw)
{
    logging_info("initializing terminal");
    self->dl11 = dl11;
    /* throttle window updates because they are very slow */
    self->cycles_since_window = 0;
    self->buffer = 0;  /* for holding LF after CR was sent */
    self->sw = sw;
    self->out = stdout;
    self->input_fd = STDIN_FILENO;
}

static struct termios saved_termios;
static int termios_saved = 0;

static void restore_terminal(void)
{
    if (termios_saved) tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
}

static void restore_and_exit(int sig)
{
    restore_terminal();
    signal(sig, SIG_DFL);
    raise(sig);
}

void Terminal_raw_input(Terminal *self)
{
    if (self->input_fd != STDIN_FILENO || !isatty(STDIN_FILENO) || termios_saved) return;
    tcgetattr(STDIN_FILENO, &saved_termios);
    termios_saved = 1;
    atexit(restore_terminal);
    signal(SIGINT, restore_and_exit);
    signal(SIGTERM, restore_and_exit);
    struct termios raw = saved_termios;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);   /* keep ISIG so Ctrl-C still stops the emulator */
    raw.c_iflag &= ~(tcflag_t)ICRNL;             /* Return arrives as CR, as from a real terminal */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void Terminal_cycle(Terminal *self)
{
    /* This is an attempt to make the terminal automatically send LF after CR.
     * If there's a character in our buffer, send it to the DL11 */
    if (self->buffer != 0) {
        if ((self->dl11->RCSR & self->dl11->RCSR_RCVR_DONE) == 0) {
            DL11_write_RBUF(self->dl11, self->buffer);
            self->buffer = 0;
        }
    }

    /* if there's a character in the dl11 transmit buffer, then eat it */
    if ((self->dl11->XCSR & self->dl11->XCSR_XMIT_RDY) == 0) {
        pyint newchar = DL11_read_XBUF(self->dl11);
        /* Sure, DL11 can send me nulls; I just won't show them. */
        if (newchar != 0) {
            char a[PYSTR];
            logging_info("dl11 read_XBUF:%s", safe_character(a, newchar));
            fputc((int)(newchar & 0377), self->out);
            fflush(self->out);
        }
    }

    /* read a character from the keyboard in a non-blocking manner,
     * if the receiver has room for it */
    if (self->input_fd >= 0 && (self->dl11->RCSR & self->dl11->RCSR_RCVR_DONE) == 0) {
        struct pollfd p = {self->input_fd, POLLIN, 0};
        if (poll(&p, 1, 0) > 0 && (p.revents & POLLIN)) {
            unsigned char key;
            ssize_t n = read(self->input_fd, &key, 1);
            if (n == 1)
                DL11_write_RBUF(self->dl11, key);
            else
                self->input_fd = -1;  /* end of input */
        }
    }
}
