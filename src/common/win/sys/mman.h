/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* sys/mman.h for Windows (src/common/win/): only the advice, which Windows
   has no use for; memory is mapped by code of its own there. */
#ifndef JANAS_WIN_SYS_MMAN_H
#define JANAS_WIN_SYS_MMAN_H
#include <stddef.h>
#define MADV_NORMAL 0
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_HUGEPAGE 14
static inline int madvise(void *addr, size_t len, int advice)
{
    (void)addr;
    (void)len;
    (void)advice;
    return 0;
}
#endif
