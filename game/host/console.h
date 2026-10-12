/* host/console.h - the game's console interface on the host, for
 * hosttest.py: output to stdout, keys from the command line. */
#ifndef CONSOLE_H
#define CONSOLE_H
#include <string.h>
void putch(int c);
int getch(void);
int key_ready(void);
void cputs(const char *s);
void putnum(int n);
void gotoxy(int row, int col);
void clear_screen(void);
void show_cursor(int on);
void __main(void);
#endif
