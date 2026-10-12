/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * stop.c - see stop.h. POSIX: SIGINT and SIGTERM blocked in every thread
 * and taken with sigwait. Windows: the console's control handler, which
 * runs in a thread of its own, sets an event the waiting thread holds.
 */
#include "common/stop.h"

#ifdef _WIN32
#include <windows.h>

static HANDLE stop_event;

static BOOL WINAPI on_control(DWORD type)
{
    (void)type; /* Ctrl+C, Ctrl+Break, the window closed, a log-off */
    SetEvent(stop_event);
    return TRUE;
}

void janas_stop_prepare(void)
{
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(on_control, TRUE);
}

void janas_stop_wait(void)
{
    WaitForSingleObject(stop_event, INFINITE);
}

#else
#include <pthread.h>
#include <signal.h>

static sigset_t stop_set;

void janas_stop_prepare(void)
{
    sigemptyset(&stop_set);
    sigaddset(&stop_set, SIGINT);
    sigaddset(&stop_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &stop_set, NULL);
    signal(SIGPIPE, SIG_IGN);
}

void janas_stop_wait(void)
{
    int sig;
    sigwait(&stop_set, &sig);
}

#endif
