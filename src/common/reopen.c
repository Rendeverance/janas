/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * reopen.c - see reopen.h. Linux: the file's entry in /proc/self/fd.
 * Windows: ReOpenFile on the descriptor's handle; a direct one is also
 * overlapped, for uring_win.c's reads that run while the caller computes.
 */
#define _GNU_SOURCE
#include "common/reopen.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>

int janas_reopen_read(int fd, int direct)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    DWORD flags = direct ? FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED : 0;
    HANDLE n = ReOpenFile(
        h, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        flags);
    if (n == INVALID_HANDLE_VALUE)
        return -1;
    int nfd = _open_osfhandle((intptr_t)n, _O_RDONLY | _O_BINARY);
    if (nfd < 0)
        CloseHandle(n);
    return nfd;
}

#else

int janas_reopen_read(int fd, int direct)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    return open(path, O_RDONLY | (direct ? O_DIRECT : 0) | O_CLOEXEC);
}

#endif
