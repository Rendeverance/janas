/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * tty.h - the terminal as the chat uses it: keys read one byte at a time,
 * the escape sequences of the arrows and the like as an xterm sends them,
 * UTF-8 both ways. POSIX: termios and the descriptors themselves. Windows:
 * the console in its virtual-terminal mode, read as UTF-16 and handed on
 * as UTF-8 (tty.c).
 */
#ifndef JANAS_CHAT_TTY_H
#define JANAS_CHAT_TTY_H

/* Once, at the start: on Windows the console made to take UTF-8 and the
   escape sequences; nothing elsewhere. */
void tty_setup(void);

/* Whether standard input and standard output are both a terminal. */
int tty_both(void);

/* The window's size; 0, or -1 when it is not known. */
int tty_size(int *rows, int *cols);

/* Raw mode (keys as they are pressed, no echo; Ctrl-C still a SIGINT) on
   or back to how it was. 0 or -1. */
int tty_raw(int on);

/* Waits up to ms (-1: as long as it takes) for a byte of input: 1 when
   there is one, 0 when none came, -1 with errno EINTR when a signal came
   or the window changed size (Windows: tty_on_resize is called first). */
int tty_wait(int ms);

/* One byte of input: 1, 0 at its end, -1 with errno (EINTR as above). */
int tty_byte(unsigned char *c);

/* Called on Windows when the console window changes size, where POSIX
   has SIGWINCH. */
extern void (*tty_on_resize)(void);

#endif
