/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* win_sockets.h (src/common/win/): Winsock under the names of POSIX's
   socket headers, and poll() over sockets, pipes and the console
   (win_posix.c). */
#ifndef JANAS_WIN_SOCKETS_H
#define JANAS_WIN_SOCKETS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#endif
typedef unsigned long nfds_t;
/* POSIX's pollfd, with a descriptor that may be -1 (Winsock's has an
   unsigned SOCKET there: its own is WSAPOLLFD) */
struct janas_pollfd {
    int fd;
    short events, revents;
};
#define pollfd janas_pollfd
/* poll over CRT descriptors (pipes, the console, files) and sockets:
   a descriptor is taken for a socket when Winsock knows it as one */
int janas_poll(struct pollfd *fds, nfds_t n, int timeout_ms);
#define poll janas_poll
#endif
