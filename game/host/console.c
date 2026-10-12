/* host/console.c - see host/console.h.  Mirrors ../console.c, so the
 * output bytes match the PDP-11 build's.  When the keys run out the
 * program exits, as the emulator run stops waiting for one. */
#include <stdio.h>
#include <stdlib.h>
#include "console.h"

static const char *keys = "";

void putch(int c) { putchar(c); }

int getch(void)
{
    if (!*keys) {
        fflush(stdout);
        exit(0);
    }
    return *keys++ & 0177;
}

int key_ready(void) { return *keys != 0; }

void cputs(const char *s)
{
    while (*s)
        putch(*s++);
}

void putnum(int n)
{
    char digits[6];
    int i = 0;
    unsigned u = n < 0 ? -(unsigned)n : (unsigned)n;
    if (n < 0)
        putch('-');
    do {
        digits[i++] = '0' + u % 10;
        u /= 10;
    } while (u);
    while (i)
        putch(digits[--i]);
}

void gotoxy(int row, int col)
{
    putch(033);
    putch('[');
    putnum(row + 1);
    putch(';');
    putnum(col + 1);
    putch('H');
}

void clear_screen(void) { cputs("\033[2J\033[H"); }

void show_cursor(int on) { cputs(on ? "\033[?25h" : "\033[?25l"); }

void __main(void) {}

int game_main(void);

int main(int argc, char **argv)
{
    if (argc > 1)
        keys = argv[1];
    return game_main();
}
