/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/* dlfcn.h for Windows (src/common/win/): a DLL loaded and its functions
   found, through LoadLibrary and GetProcAddress (win_posix.c). */
#ifndef JANAS_WIN_DLFCN_H
#define JANAS_WIN_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 0x100
void *dlopen(const char *file, int mode);
void *dlsym(void *handle, const char *name);
int dlclose(void *handle);
char *dlerror(void);
#endif
