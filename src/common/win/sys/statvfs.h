/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* sys/statvfs.h for Windows (src/common/win/): a file system's room, from
   GetDiskFreeSpaceEx (win_posix.c). */
#ifndef JANAS_WIN_SYS_STATVFS_H
#define JANAS_WIN_SYS_STATVFS_H
struct statvfs {
    unsigned long long f_bsize, f_frsize, f_blocks, f_bfree, f_bavail;
};
int statvfs(const char *path, struct statvfs *s);
int fstatvfs(int fd, struct statvfs *s);
#endif
