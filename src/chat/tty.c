/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * tty.c - see tty.h.
 */
#include "tty.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>

void (*tty_on_resize)(void);

#ifdef _WIN32
#include <windows.h>
#include <io.h>

static HANDLE in_h, out_h;
static DWORD saved_in;
static int raw_on;
/* the UTF-8 bytes of a character read and not yet handed out */
static unsigned char queue[8];
static int q_at, q_n;

void tty_setup(void)
{
    in_h = GetStdHandle(STD_INPUT_HANDLE);
    out_h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    /* the escape sequences understood, and the wrap at the last column
       left pending as an xterm does, so that a full line does not take
       two */
    if (GetConsoleMode(out_h, &m))
        SetConsoleMode(out_h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                  DISABLE_NEWLINE_AUTO_RETURN);
    HANDLE err_h = GetStdHandle(STD_ERROR_HANDLE);
    if (GetConsoleMode(err_h, &m))
        SetConsoleMode(err_h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static int is_console(HANDLE h)
{
    DWORD m;
    return GetConsoleMode(h, &m) != 0;
}

int tty_both(void)
{
    return is_console(GetStdHandle(STD_INPUT_HANDLE)) &&
           is_console(GetStdHandle(STD_OUTPUT_HANDLE));
}

int tty_size(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO i;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &i))
        return -1;
    *rows = i.srWindow.Bottom - i.srWindow.Top + 1;
    *cols = i.srWindow.Right - i.srWindow.Left + 1;
    return 0;
}

int tty_raw(int on)
{
    if (!in_h)
        tty_setup();
    if (on == raw_on)
        return 0;
    if (on) {
        if (!GetConsoleMode(in_h, &saved_in))
            return -1;
        /* keys one by one as an xterm sends them; Ctrl-C still the
           console's, which the C runtime turns into SIGINT */
        DWORD m = (saved_in & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                       ENABLE_QUICK_EDIT_MODE)) |
                  ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_PROCESSED_INPUT |
                  ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
        if (!SetConsoleMode(in_h, m))
            return -1;
    } else if (!SetConsoleMode(in_h, saved_in))
        return -1;
    raw_on = on;
    return 0;
}

/* Takes out of the console's queue what gives no character (keys let go,
   Shift alone, the mouse, the focus); a change of size is reported. 1 when
   a character waits, 0 when nothing does, -1 when the window changed. */
static int console_pending(void)
{
    for (;;) {
        INPUT_RECORD r;
        DWORD n = 0;
        if (!PeekConsoleInputW(in_h, &r, 1, &n) || n == 0)
            return 0;
        if (r.EventType == KEY_EVENT && r.Event.KeyEvent.bKeyDown &&
            r.Event.KeyEvent.uChar.UnicodeChar)
            return 1;
        ReadConsoleInputW(in_h, &r, 1, &n);
        if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            if (tty_on_resize)
                tty_on_resize();
            return -1;
        }
    }
}

int tty_wait(int ms)
{
    if (!in_h)
        tty_setup();
    if (q_at < q_n)
        return 1;
    if (!is_console(in_h)) {
        struct pollfd pf = {.fd = 0, .events = POLLIN};
        return poll(&pf, 1, ms);
    }
    ULONGLONG end = GetTickCount64() + (ULONGLONG)(ms < 0 ? 0 : ms);
    for (;;) {
        int p = console_pending();
        if (p > 0)
            return 1;
        if (p < 0) {
            errno = EINTR;
            return -1;
        }
        ULONGLONG now = GetTickCount64();
        if (ms >= 0 && now >= end)
            return 0;
        DWORD left = ms < 0 ? INFINITE : (DWORD)(end - now);
        if (WaitForSingleObject(in_h, left) == WAIT_TIMEOUT)
            return 0;
    }
}

int tty_byte(unsigned char *c)
{
    if (!in_h)
        tty_setup();
    if (q_at < q_n) {
        *c = queue[q_at++];
        return 1;
    }
    if (!is_console(in_h)) {
        int r = _read(0, c, 1);
        return r < 0 ? -1 : r;
    }
    for (;;) {
        int p = tty_wait(-1);
        if (p < 0)
            return -1;
        wchar_t w[2];
        DWORD n = 0;
        if (!ReadConsoleW(in_h, w, 1, &n, NULL)) {
            errno = GetLastError() == ERROR_OPERATION_ABORTED ? EINTR : EIO;
            return -1;
        }
        if (n == 0)
            continue;
        int units = 1;
        /* a character beyond the first plane comes as two halves */
        if (w[0] >= 0xD800 && w[0] <= 0xDBFF &&
            ReadConsoleW(in_h, w + 1, 1, &n, NULL) && n == 1)
            units = 2;
        if (w[0] == 26 && !raw_on)
            return 0; /* Ctrl-Z: the end of the input, as Windows has it */
        int len = WideCharToMultiByte(CP_UTF8, 0, w, units, (char *)queue,
                                      sizeof queue, NULL, NULL);
        if (len <= 0)
            continue;
        q_at = 1;
        q_n = len;
        *c = queue[0];
        return 1;
    }
}

#else
#include <sys/ioctl.h>
#include <termios.h>

static struct termios saved;
static int raw_on;

void tty_setup(void)
{
}

int tty_both(void)
{
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

int tty_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
        return -1;
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

int tty_raw(int on)
{
    if (on == raw_on)
        return 0;
    if (on) {
        if (tcgetattr(STDIN_FILENO, &saved) != 0)
            return -1;
        struct termios r = saved;
        r.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
        r.c_cc[VMIN] = 1;
        r.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &r) != 0)
            return -1;
    } else if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved) != 0)
        return -1;
    raw_on = on;
    return 0;
}

int tty_wait(int ms)
{
    struct pollfd pf = {STDIN_FILENO, POLLIN, 0};
    return poll(&pf, 1, ms);
}

int tty_byte(unsigned char *c)
{
    ssize_t r = read(STDIN_FILENO, c, 1);
    return r < 0 ? -1 : (int)r;
}

#endif
