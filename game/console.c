/* console.c - see console.h */
#include "console.h"

void putch(int c)
{
    while (!(XCSR & DL11_READY))
        ;
    XBUF = c;
}

int getch(void)
{
    while (!(RCSR & DL11_READY))
        ;
    return RBUF & 0177;
}

int key_ready(void)
{
    return RCSR & DL11_READY;
}

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

/* ESC [ row ; col H, counting from 1 */
void gotoxy(int row, int col)
{
    putch(033);
    putch('[');
    putnum(row + 1);
    putch(';');
    putnum(col + 1);
    putch('H');
}

void clear_screen(void)
{
    cputs("\033[2J\033[H");
}

void show_cursor(int on)
{
    cputs(on ? "\033[?25h" : "\033[?25l");
}

void *memset(void *s, int c, unsigned n)
{
    char *p = s;
    while (n--)
        *p++ = c;
    return s;
}

void *memcpy(void *d, const void *s, unsigned n)
{
    char *p = d;
    const char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

/* GCC's a.out targets have main call __main first, to run constructors.
 * libgcc's __main needs atexit; a C program has nothing to construct. */
void __main(void)
{
}
