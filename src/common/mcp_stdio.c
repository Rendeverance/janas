/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * mcp_stdio.c - an MCP server as a child process (see mcp_stdio.h).
 */
#include "common/mcp_stdio.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LINE_MAX_BYTES ((size_t)64 << 20)

int janas_mcp_proc_start(struct janas_mcp_proc *p, char *const argv[],
                         char *const env[], const char *log_path, char *err,
                         size_t err_len)
{
    memset(p, 0, sizeof(*p));
    p->in = p->out = -1;
    int to[2] = {-1, -1}, from[2] = {-1, -1};
    int log = -1, rc = -1;
    if (!argv || !argv[0] || !*argv[0]) {
        snprintf(err, err_len, "no command");
        return -1;
    }
    if (janas_pipe(to) != 0 || janas_pipe(from) != 0) {
        snprintf(err, err_len, "pipe: %s", strerror(errno));
        goto out;
    }
    if (log_path)
        log = open(log_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (log < 0)
        log = janas_null_fd(1);
    if (log < 0) {
        snprintf(err, err_len, "no null device");
        goto out;
    }
    /* a group of its own, the signals as a new program has them */
    if (janas_proc_start(&p->proc, (const char *const *)argv,
                         (const char *const *)env, NULL, to[0], from[1], log,
                         JANAS_PROC_GROUP, err, err_len) != 0)
        goto out;
    p->in = to[1];
    p->out = from[0];
    to[1] = from[0] = -1;
    rc = 0;
out:
    for (int i = 0; i < 2; i++) {
        if (to[i] >= 0)
            close(to[i]);
        if (from[i] >= 0)
            close(from[i]);
    }
    if (log >= 0)
        close(log);
    return rc;
}

/* A write to a server that has gone raises SIGPIPE, which would end the
   chat: it is held back on this thread for the write, and taken away if it
   came, so the write only fails. */
int janas_mcp_proc_send(struct janas_mcp_proc *p, const char *s, size_t n)
{
    if (p->in < 0)
        return -1;
#ifdef _WIN32
    /* no signal there: a write to a pipe nobody reads only fails */
    while (n > 0) {
        int w = write(p->in, s, n > 0x40000000u ? 0x40000000u : (unsigned)n);
        if (w <= 0)
            return -1;
        s += w;
        n -= (size_t)w;
    }
    return 0;
#else
    sigset_t pipe_set, old;
    sigemptyset(&pipe_set);
    sigaddset(&pipe_set, SIGPIPE);
    sigset_t pending;
    sigpending(&pending);
    int was_pending = sigismember(&pending, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe_set, &old);
    int rc = 0;
    while (n > 0) {
        ssize_t w = write(p->in, s, n);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0) {
            rc = -1;
            break;
        }
        s += w;
        n -= (size_t)w;
    }
    if (rc && !was_pending) {
        struct timespec zero = {0, 0};
        while (sigtimedwait(&pipe_set, NULL, &zero) < 0 && errno == EINTR)
            ;
    }
    pthread_sigmask(SIG_SETMASK, &old, NULL);
    return rc;
#endif
}

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e3 + (double)t.tv_nsec / 1e6;
}

int janas_mcp_proc_line(struct janas_mcp_proc *p, int timeout_ms,
                        const char **line, size_t *n)
{
    double end = now_ms() + timeout_ms;
    size_t scanned = p->start;
    for (;;) {
        char *nl = p->n > scanned
                       ? memchr(p->buf + scanned, '\n', p->n - scanned)
                       : NULL;
        if (nl) {
            size_t len = (size_t)(nl - (p->buf + p->start));
            *line = p->buf + p->start;
            if (len && (*line)[len - 1] == '\r')
                len--;
            *n = len;
            p->start = (size_t)(nl - p->buf) + 1;
            return 1;
        }
        scanned = p->n;
        if (p->eof || p->out < 0)
            return -1;
        if (p->n - p->start > LINE_MAX_BYTES)
            return -1;
        if (p->start > 0 && p->start == p->n) {
            p->n = p->start = scanned = 0;
        } else if (p->start > p->cap / 2) { /* the lines taken go */
            memmove(p->buf, p->buf + p->start, p->n - p->start);
            p->n -= p->start;
            scanned -= p->start;
            p->start = 0;
        }
        if (p->cap - p->n < 65536) {
            size_t cap = p->cap ? 2 * p->cap : 131072;
            char *b = realloc(p->buf, cap);
            if (!b)
                return -1;
            p->buf = b;
            p->cap = cap;
        }
        /* one look at least, also with no time left */
        double left = end - now_ms();
        struct pollfd pf = {.fd = p->out, .events = POLLIN};
        int r = poll(&pf, 1,
                     left <= 0    ? 0
                     : left > 1e9 ? 1000000000
                                  : (int)left + 1);
        if (r < 0 && errno == EINTR)
            return 0; /* a signal: the caller looks at why */
        if (r <= 0) {
            if (now_ms() >= end)
                return 0;
            continue;
        }
        ssize_t got = read(p->out, p->buf + p->n, p->cap - p->n);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            p->eof = 1;
        else
            p->n += (size_t)got;
    }
}

void janas_mcp_proc_stop(struct janas_mcp_proc *p)
{
    if (p->in >= 0)
        close(p->in);
    p->in = -1;
    if (p->proc.id && janas_proc_wait(&p->proc, 1000, NULL) == 0) {
        janas_proc_kill(&p->proc, 0);
        if (janas_proc_wait(&p->proc, 2000, NULL) == 0) {
            janas_proc_kill(&p->proc, 1);
            janas_proc_wait(&p->proc, -1, NULL);
        }
    }
    memset(&p->proc, 0, sizeof p->proc);
    if (p->out >= 0)
        close(p->out);
    p->out = -1;
    free(p->buf);
    p->buf = NULL;
    p->n = p->cap = p->start = 0;
}
