/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * sock.c - see sock.h.
 */
#define _GNU_SOURCE
#include "common/sock.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>

static INIT_ONCE started = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK start(PINIT_ONCE once, PVOID arg, PVOID *ctx)
{
    (void)once, (void)arg, (void)ctx;
    WSADATA d;
    WSAStartup(MAKEWORD(2, 2), &d);
    return TRUE;
}

void janas_sock_init(void)
{
    InitOnceExecuteOnce(&started, start, NULL, NULL);
}

/* Winsock's last error in errno, under POSIX's name. */
static void wsa_errno(void)
{
    switch (WSAGetLastError()) {
    case WSAEWOULDBLOCK:
        errno = EWOULDBLOCK;
        break;
    case WSAEINPROGRESS:
        errno = EINPROGRESS;
        break;
    case WSAEINTR:
        errno = EINTR;
        break;
    case WSAECONNRESET:
        errno = ECONNRESET;
        break;
    case WSAECONNREFUSED:
        errno = ECONNREFUSED;
        break;
    case WSAETIMEDOUT:
        errno = ETIMEDOUT;
        break;
    case WSAENOTCONN:
        errno = ENOTCONN;
        break;
    case WSAEHOSTUNREACH:
        errno = EHOSTUNREACH;
        break;
    case WSAENETUNREACH:
        errno = ENETUNREACH;
        break;
    default:
        errno = EIO;
    }
}

int janas_sock_open(int family, int type, int protocol)
{
    janas_sock_init();
    SOCKET s = WSASocketW(family, type, protocol, NULL, 0,
                          WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (s == INVALID_SOCKET) {
        wsa_errno();
        return -1;
    }
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
    return (int)s;
}

int janas_sock_connecting(void)
{
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}

long janas_sock_recv(int fd, void *p, size_t n)
{
    int r = recv((SOCKET)fd, p, n > 0x40000000u ? 0x40000000 : (int)n, 0);
    if (r == SOCKET_ERROR) {
        wsa_errno();
        return -1;
    }
    return r;
}

long janas_sock_send(int fd, const void *p, size_t n)
{
    int w = send((SOCKET)fd, p, n > 0x40000000u ? 0x40000000 : (int)n, 0);
    if (w == SOCKET_ERROR) {
        wsa_errno();
        return -1;
    }
    return w;
}

int janas_sock_close(int fd)
{
    return closesocket((SOCKET)fd) == 0 ? 0 : -1;
}

#else

void janas_sock_init(void)
{
}

int janas_sock_open(int family, int type, int protocol)
{
    return socket(family, type | SOCK_CLOEXEC | SOCK_NONBLOCK, protocol);
}

int janas_sock_connecting(void)
{
    return errno == EINPROGRESS;
}

long janas_sock_recv(int fd, void *p, size_t n)
{
    return read(fd, p, n);
}

long janas_sock_send(int fd, const void *p, size_t n)
{
    return send(fd, p, n, MSG_NOSIGNAL);
}

int janas_sock_close(int fd)
{
    return close(fd);
}

#endif
