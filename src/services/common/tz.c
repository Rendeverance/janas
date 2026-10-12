/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * tz.c - see tz.h.
 */
#define _GNU_SOURCE
#include "tz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

/* ICU's C calendar, as Windows's icu.dll exports it (no version in the
   names there) */
typedef uint16_t UChar;
typedef double UDate;
typedef int UErrorCode;
typedef void UCalendar;
enum {
    F_YEAR = 1,
    F_MONTH = 2,
    F_DATE = 5,
    F_DAY_OF_YEAR = 6,
    F_DAY_OF_WEEK = 7,
    F_HOUR_OF_DAY = 11,
    F_MINUTE = 12,
    F_SECOND = 13,
    F_MILLISECOND = 14,
    F_DST_OFFSET = 16
};
enum { SHORT_STANDARD = 1, SHORT_DST = 3 };

static struct {
    UCalendar *(*open)(const UChar *, int32_t, const char *, int, UErrorCode *);
    void (*close)(UCalendar *);
    void (*set_millis)(UCalendar *, UDate, UErrorCode *);
    UDate (*get_millis)(const UCalendar *, UErrorCode *);
    int32_t (*get)(const UCalendar *, int, UErrorCode *);
    void (*set)(UCalendar *, int, int32_t);
    void (*set_date_time)(UCalendar *, int32_t, int32_t, int32_t, int32_t,
                          int32_t, int32_t, UErrorCode *);
    int32_t (*zone_name)(const UCalendar *, int, const char *, UChar *, int32_t,
                         UErrorCode *);
    int32_t (*default_zone)(UChar *, int32_t, UErrorCode *);
} icu;
static INIT_ONCE icu_once = INIT_ONCE_STATIC_INIT;
static int icu_ok;

/* A function of the DLL into its field: a FARPROC's bytes, as a pointer
   to a function of another type has them. */
static int icu_fn(void *field, HMODULE m, const char *name)
{
    FARPROC f = GetProcAddress(m, name);
    if (!f)
        return 0;
    memcpy(field, &f, sizeof f);
    return 1;
}
#define ICU_FN(field, name) icu_fn(&icu.field, m, name)

static BOOL CALLBACK icu_load(PINIT_ONCE once, PVOID arg, PVOID *ctx)
{
    (void)once, (void)arg, (void)ctx;
    HMODULE m = LoadLibraryExA("icu.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    icu_ok = m && ICU_FN(open, "ucal_open") && ICU_FN(close, "ucal_close") &&
             ICU_FN(set_millis, "ucal_setMillis") &&
             ICU_FN(get_millis, "ucal_getMillis") && ICU_FN(get, "ucal_get") &&
             ICU_FN(set, "ucal_set") &&
             ICU_FN(set_date_time, "ucal_setDateTime") &&
             ICU_FN(zone_name, "ucal_getTimeZoneDisplayName") &&
             ICU_FN(default_zone, "ucal_getDefaultTimeZone");
    return TRUE;
}

/* A Gregorian calendar in the zone tz (NULL or "": the machine's). */
static UCalendar *calendar(const char *tz)
{
    InitOnceExecuteOnce(&icu_once, icu_load, NULL, NULL);
    if (!icu_ok)
        return NULL;
    UChar id[96];
    int n = 0;
    for (; tz && tz[n] && n < 95; n++)
        id[n] = (unsigned char)tz[n];
    UErrorCode e = 0;
    UCalendar *c = icu.open(n ? id : NULL, n ? n : -1, "en_US", 1, &e);
    if (e > 0) {
        if (c)
            icu.close(c);
        return NULL;
    }
    return c;
}

static void utc_tm(time_t t, struct tm *out, char *abbr, size_t abbr_len)
{
    gmtime_r(&t, out);
    if (abbr)
        snprintf(abbr, abbr_len, "UTC");
}

int janas_tz_local(const char *tz, time_t t, struct tm *out, char *abbr,
                   size_t abbr_len)
{
    UCalendar *c = calendar(tz);
    if (!c) {
        utc_tm(t, out, abbr, abbr_len);
        return -1;
    }
    UErrorCode e = 0;
    icu.set_millis(c, (UDate)t * 1000.0, &e);
    memset(out, 0, sizeof *out);
    out->tm_year = icu.get(c, F_YEAR, &e) - 1900;
    out->tm_mon = icu.get(c, F_MONTH, &e);
    out->tm_mday = icu.get(c, F_DATE, &e);
    out->tm_yday = icu.get(c, F_DAY_OF_YEAR, &e) - 1;
    out->tm_wday = icu.get(c, F_DAY_OF_WEEK, &e) - 1;
    out->tm_hour = icu.get(c, F_HOUR_OF_DAY, &e);
    out->tm_min = icu.get(c, F_MINUTE, &e);
    out->tm_sec = icu.get(c, F_SECOND, &e);
    out->tm_isdst = icu.get(c, F_DST_OFFSET, &e) != 0;
    if (abbr && abbr_len) {
        /* ICU gives a zone's short name ("CEST") only in the languages
           where it is in common use, and "GMT+2" elsewhere: the English of
           the places that use it, the first that has one */
        static const char *const langs[] = {"en_US", "en_GB", "en_IE", "en_AU",
                                            "en_NZ", "en_IN", "en_ZA", "en_CA"};
        abbr[0] = 0;
        for (size_t l = 0; l < sizeof langs / sizeof *langs; l++) {
            UChar w[32];
            UErrorCode le = 0;
            int32_t n =
                icu.zone_name(c, out->tm_isdst ? SHORT_DST : SHORT_STANDARD,
                              langs[l], w, 32, &le);
            if (le > 0 || n <= 0)
                continue;
            size_t k = 0;
            for (int32_t i = 0; i < n && i < 32 && k + 1 < abbr_len; i++)
                if (w[i] < 0x80)
                    abbr[k++] = (char)w[i];
            abbr[k] = 0;
            if (strncmp(abbr, "GMT", 3) != 0 && strncmp(abbr, "UTC", 3) != 0)
                break; /* a name of its own; "GMT+2" is kept if none has */
        }
    }
    icu.close(c);
    if (e > 0) {
        utc_tm(t, out, abbr, abbr_len);
        return -1;
    }
    return 0;
}

time_t janas_tz_mktime(const char *tz, struct tm *lt)
{
    UCalendar *c = calendar(tz);
    if (!c)
        return (time_t)-1;
    UErrorCode e = 0;
    /* lenient: a 32nd day is the 1st of the next month, as mktime has it */
    icu.set_date_time(c, lt->tm_year + 1900, lt->tm_mon, lt->tm_mday,
                      lt->tm_hour, lt->tm_min, lt->tm_sec, &e);
    icu.set(c, F_MILLISECOND, 0);
    UDate ms = icu.get_millis(c, &e);
    icu.close(c);
    if (e > 0)
        return (time_t)-1;
    time_t t = (time_t)(ms / 1000.0);
    janas_tz_local(tz, t, lt, NULL, 0);
    return t;
}

int janas_tz_machine(char *buf, size_t len)
{
    InitOnceExecuteOnce(&icu_once, icu_load, NULL, NULL);
    if (!icu_ok || len == 0)
        return -1;
    UChar w[96];
    UErrorCode e = 0;
    int32_t n = icu.default_zone(w, 96, &e);
    if (e > 0 || n <= 0 || n >= 96)
        return -1;
    size_t k = 0;
    for (int32_t i = 0; i < n && k + 1 < len; i++)
        buf[k++] = w[i] < 0x80 ? (char)w[i] : '?';
    buf[k] = 0;
    return 0;
}

#else
#include <unistd.h>

/* The zone set for a while and the one before it put back: what the
   services did each on its own until the C library's other systems came
   in. */
static char *zone_set(const char *tz)
{
    char *old = getenv("TZ") ? strdup(getenv("TZ")) : NULL;
    if (tz && *tz)
        setenv("TZ", tz, 1);
    tzset();
    return old;
}

static void zone_back(const char *tz, char *old)
{
    if (tz && *tz) {
        if (old)
            setenv("TZ", old, 1);
        else
            unsetenv("TZ");
        tzset();
    }
    free(old);
}

int janas_tz_local(const char *tz, time_t t, struct tm *out, char *abbr,
                   size_t abbr_len)
{
    char *old = zone_set(tz);
    int rc = localtime_r(&t, out) ? 0 : -1;
    if (abbr && abbr_len)
        snprintf(abbr, abbr_len, "%s",
                 rc == 0 && out->tm_zone ? out->tm_zone : "");
    zone_back(tz, old);
    return rc;
}

time_t janas_tz_mktime(const char *tz, struct tm *lt)
{
    char *old = zone_set(tz);
    time_t t = mktime(lt);
    zone_back(tz, old);
    return t;
}

int janas_tz_machine(char *buf, size_t len)
{
    char path[256] = "";
    const char *tz = getenv("TZ");
    if (tz && *tz) {
        snprintf(path, sizeof path, "%s", tz[0] == ':' ? tz + 1 : tz);
    } else {
        ssize_t n = readlink("/etc/localtime", path, sizeof path - 1);
        if (n <= 0)
            return -1;
        path[n] = 0;
    }
    const char *z = strstr(path, "zoneinfo/");
    snprintf(buf, len, "%s", z ? z + 9 : path); /* "Europe/Rome" */
    return 0;
}

#endif
