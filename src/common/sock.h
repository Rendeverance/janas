/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * sock.h - the few things a TCP socket does differently on Windows: it is
 * not a file descriptor there (recv and send, not read and write; closed
 * with closesocket), its errors come from WSAGetLastError, and Winsock
 * must be started before any of it, name lookups included. These calls
 * leave the error in errno, with POSIX's names, on every system.
 */
#ifndef JANAS_COMMON_SOCK_H
#define JANAS_COMMON_SOCK_H

#include <stddef.h>

/* Winsock started, once (nothing elsewhere). Before getaddrinfo too. */
void janas_sock_init(void);

/* A socket that does not block and is not passed on to programs started
   later, or -1. */
int janas_sock_open(int family, int type, int protocol);

/* Whether the connect() that just failed is only under way (EINPROGRESS;
   Windows: WSAEWOULDBLOCK). */
int janas_sock_connecting(void);

long janas_sock_recv(int fd, void *p, size_t n);
long janas_sock_send(int fd, const void *p, size_t n);
int janas_sock_close(int fd);

#endif
