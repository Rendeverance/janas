/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * model_pick.h - a model by name, for the programs that open one
 * (janas-chat, janas-bench): a path, a name of the catalog
 * (src/common/catalog.c), or none, and then a list to choose from. A model
 * of the catalog that is not there yet is downloaded and converted by
 * janas-get, with its prediction file or draft, once the user says so.
 */
#ifndef JANAS_COMMON_MODEL_PICK_H
#define JANAS_COMMON_MODEL_PICK_H

#include <stddef.h>

#include "common/catalog.h"

struct janas_model_pick {
    char path[4400]; /* the .jns to open */
    char dir[4096];  /* the folder it is in */
    char key[128];   /* its settings file: the catalog name, or the file's */
    const struct janas_model_entry *entry; /* NULL: not in the catalog */
};

/* arg: a path, a catalog name, or NULL for the list; prog names the
   program in what is printed. 0, or -1 with the reason printed. */
int janas_model_pick(const char *prog, const char *arg,
                     struct janas_model_pick *mp);

/* The draft the catalog names for the model, if its file is beside it:
   the path in buf, or NULL. */
const char *janas_model_pick_draft(const struct janas_model_pick *mp, char *buf,
                                   size_t len);

/* The model's prediction file: for a model of the catalog the one the
   catalog names, beside it (none if it names none: a file that merely fits
   may be another model's - Qwen3-Coder-Next took Qwen3-Next-80B's, and
   drafted slower than from the conversation); otherwise the one
   janas_llm_find_mtp finds beside it. The path in buf, or NULL. */
const char *janas_model_pick_mtp(const struct janas_model_pick *mp, char *buf,
                                 size_t len);

/* A question answered y or n on standard input (before a terminal of the
   program's own is set up); anything but y is no. */
int janas_ask_yes(const char *question);

#endif
