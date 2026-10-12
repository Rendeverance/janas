/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* sys/utsname.h for Windows (src/common/win/): the system's name and
   version, from RtlGetVersion (win_posix.c). */
#ifndef JANAS_WIN_SYS_UTSNAME_H
#define JANAS_WIN_SYS_UTSNAME_H
struct utsname {
    char sysname[65], nodename[65], release[65], version[65], machine[65];
};
int uname(struct utsname *u);
#endif
