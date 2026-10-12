/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * tz.h - times in a zone named as IANA names them ("Europe/Rome"), for
 * Janas's services. POSIX: the C library's own, through TZ. Windows,
 * whose C library knows no IANA names: the ICU that comes with Windows 10
 * and 11 (icu.dll).
 */
#ifndef JANAS_SERVICES_TZ_H
#define JANAS_SERVICES_TZ_H

#include <stddef.h>
#include <time.h>

/* The time t in the zone tz (NULL or "": the machine's), into *out, and
   the zone's abbreviation there and then ("CEST") into abbr when it is not
   NULL. 0, or -1 (*out then in UTC). */
int janas_tz_local(const char *tz, time_t t, struct tm *out, char *abbr,
                   size_t abbr_len);

/* The instant of the local time *lt in the zone tz (NULL or "": the
   machine's), as mktime would give it there; *lt normalized as mktime
   leaves it. -1 when it cannot be told. */
time_t janas_tz_mktime(const char *tz, struct tm *lt);

/* The machine's zone as an IANA name ("Europe/Rome"). 0, or -1 when it is
   not known. */
int janas_tz_machine(char *buf, size_t len);

#endif
