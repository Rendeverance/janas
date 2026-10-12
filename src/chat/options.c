/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * options.c - janas-chat's options: those of the command line and of its
 * settings files (see options.h).
 */
#include "options.h"

#include <string.h>

const char *chat_words_last(int n, char *const *w, const char *opt)
{
    const char *v = NULL;
    for (int i = 0; i + 1 < n; i++)
        if (strcmp(w[i], opt) == 0)
            v = w[i + 1];
    return v;
}

int chat_words_any(int n, char *const *w, const char *const *opts)
{
    for (int i = 0; i < n; i++)
        for (int k = 0; opts[k]; k++)
            if (strcmp(w[i], opts[k]) == 0)
                return 1;
    return 0;
}
