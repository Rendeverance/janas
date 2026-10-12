/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * uring_win.c - uring.h on Windows, with overlapped reads: a read starts
 * as soon as it is queued (ReadFile on a handle opened for overlapped and
 * unbuffered I/O, reopen.c) and runs while the caller computes;
 * janas_uring_wait collects every one. As with io_uring, no thread is
 * woken to begin a read. A read that returns fewer bytes than asked is
 * started again for what is missing.
 */
#ifdef _WIN32
#include "common/uring.h"

#include <windows.h>
#include <io.h>
#include <stdlib.h>

struct request {
    OVERLAPPED o; /* the system's while the read runs: it must live here */
    HANDLE h;
    uint8_t *dst;
    uint64_t offset, bytes, done;
};

struct janas_uring {
    unsigned entries, n;
    int failed;
    struct request *req;
    HANDLE *events;
};

void janas_uring_destroy(struct janas_uring *u)
{
    if (!u)
        return;
    janas_uring_wait(u);
    for (unsigned i = 0; i < u->entries; i++)
        if (u->events[i])
            CloseHandle(u->events[i]);
    free(u->events);
    free(u->req);
    free(u);
}

struct janas_uring *janas_uring_create(unsigned entries)
{
    struct janas_uring *u = calloc(1, sizeof *u);
    if (!u || entries == 0) {
        free(u);
        return NULL;
    }
    u->entries = entries;
    u->req = calloc(entries, sizeof *u->req);
    u->events = calloc(entries, sizeof *u->events);
    if (!u->req || !u->events) {
        free(u->req);
        free(u->events);
        free(u);
        return NULL;
    }
    for (unsigned i = 0; i < entries; i++) {
        u->events[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!u->events[i]) {
            janas_uring_destroy(u);
            return NULL;
        }
    }
    return u;
}

/* Starts (or starts again) the read of what request i still misses. */
static int start(struct janas_uring *u, unsigned i)
{
    struct request *r = &u->req[i];
    uint64_t at = r->offset + r->done, left = r->bytes - r->done;
    DWORD chunk = left > 0x40000000u ? 0x40000000u : (DWORD)left;
    ResetEvent(u->events[i]);
    r->o = (OVERLAPPED){.Offset = (DWORD)at,
                        .OffsetHigh = (DWORD)(at >> 32),
                        .hEvent = u->events[i]};
    if (ReadFile(r->h, r->dst + r->done, chunk, NULL, &r->o))
        return 0; /* done already: the result is collected as any other */
    return GetLastError() == ERROR_IO_PENDING ? 0 : -1;
}

int janas_uring_read(struct janas_uring *u, int fd, void *dst, uint64_t bytes,
                     uint64_t offset)
{
    if (u->n == u->entries)
        return -1;
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    unsigned i = u->n;
    u->req[i] =
        (struct request){.h = h, .dst = dst, .offset = offset, .bytes = bytes};
    if (bytes == 0)
        return 0;
    if (start(u, i) != 0)
        return -1;
    u->n++;
    return 0;
}

int janas_uring_submit(struct janas_uring *u)
{
    (void)u; /* each read began when it was queued */
    return 0;
}

int janas_uring_wait(struct janas_uring *u)
{
    for (unsigned i = 0; i < u->n; i++) {
        struct request *r = &u->req[i];
        for (;;) {
            DWORD got = 0;
            if (!GetOverlappedResult(r->h, &r->o, &got, TRUE) || got == 0) {
                u->failed = 1;
                break;
            }
            r->done += got;
            if (r->done >= r->bytes)
                break;
            if (start(u, i) != 0) {
                u->failed = 1;
                break;
            }
        }
    }
    int rc = u->failed ? -1 : 0;
    u->n = 0;
    u->failed = 0;
    return rc;
}

unsigned janas_uring_inflight(const struct janas_uring *u)
{
    return u->n;
}

#endif /* _WIN32 */
