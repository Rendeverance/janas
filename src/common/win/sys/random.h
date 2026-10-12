/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* sys/random.h for Windows (src/common/win/): the system's random bytes,
   from BCryptGenRandom (win_posix.c). */
#ifndef JANAS_WIN_SYS_RANDOM_H
#define JANAS_WIN_SYS_RANDOM_H
#include <stddef.h>
#include <sys/types.h>
ssize_t getrandom(void *buf, size_t n, unsigned flags);
#endif
