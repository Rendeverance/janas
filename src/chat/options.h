/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * options.h - janas-chat's options: those of the command line and of its
 * settings files, looked through for an option.
 */
#ifndef JANAS_CHAT_OPTIONS_H
#define JANAS_CHAT_OPTIONS_H

/* The value of the last opt among n words, or NULL. */
const char *chat_words_last(int n, char *const *w, const char *opt);

/* 1 if one of the n words is one of opts (NULL-terminated). */
int chat_words_any(int n, char *const *w, const char *const *opts);

#endif
