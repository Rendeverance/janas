/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * stop.h - a program that runs until it is asked to stop: Ctrl+C, a
 * termination (SIGTERM), on Windows the console closed or the user
 * logging off. The request is awaited by one thread; the others never
 * see it.
 */
#ifndef JANAS_COMMON_STOP_H
#define JANAS_COMMON_STOP_H

/* Before any thread is made: the requests to stop held back from every
   thread but the one that awaits them; a write to a closed connection an
   error, not the end of the process (SIGPIPE ignored). */
void janas_stop_prepare(void);

/* Waits for a request to stop. */
void janas_stop_wait(void);

#endif
