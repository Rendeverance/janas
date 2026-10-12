/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * reopen.h - a file opened again from a descriptor of it, for reading:
 * with direct I/O (O_DIRECT, Windows's unbuffered and overlapped reads) or
 * through the system's cache. The new descriptor is independent of the
 * old one (its own position, its own flags).
 */
#ifndef JANAS_COMMON_REOPEN_H
#define JANAS_COMMON_REOPEN_H

/* A new read-only descriptor of fd's file, or -1. Direct: the reads must
   be aligned as O_DIRECT's (offset, length and buffer). */
int janas_reopen_read(int fd, int direct);

#endif
