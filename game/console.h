/* console.h - the console terminal, through the DL11 at 0o177560, with no
 * operating system: polled input and output, and ANSI (VT100) screen
 * control, which the terminal window the emulator runs in interprets. */
#ifndef CONSOLE_H
#define CONSOLE_H

/* DL11 registers */
#define RCSR (*(volatile unsigned *)0177560) /* receiver status: bit 7 = character ready */
#define RBUF (*(volatile unsigned *)0177562) /* received character; reading clears bit 7 */
#define XCSR (*(volatile unsigned *)0177564) /* transmitter status: bit 7 = ready */
#define XBUF (*(volatile unsigned *)0177566) /* character to send */
#define DL11_READY 0200

void putch(int c);
int getch(void);       /* waits for a key */
int key_ready(void);   /* nonzero if a key is waiting */
void cputs(const char *s);
void putnum(int n);    /* signed decimal */

/* screen, with rows and columns from 0 */
void gotoxy(int row, int col);
void clear_screen(void);
void show_cursor(int on);

/* the C library functions GCC may call for structure copies and clears */
void *memset(void *s, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);

/* called by main before anything else (see console.c) */
void __main(void);

#endif
