
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-23.
//

#include "time-zone.h"

#include <sys/stat.h>
#include <bits/stat.h>

#include "str.h"
#include "date.h"
#include "hash-table.h"
#include "bytes.h"
#include "thread.h"
#include "atomic.h"
#include "file-utils.h"
#include "mapped-file.h"

#define TRANSITION(n)         c_array_index (tz->transitions, Transition, n)
#define TRANSITION_INFO(n)    c_array_index (tz->tInfo, TransitionInfo, n)


typedef struct { char bytes[8]; } cint64_be;
typedef struct { char bytes[4]; } cint32_be;
typedef struct { char bytes[4]; } cuint32_be;

static inline cint64 cint64_from_be (const cint64_be be)
{
    cint64 tmp; memcpy (&tmp, &be, sizeof tmp); return C_INT64_FROM_BE (tmp);
}

static inline cint32 cint32_from_be (const cint32_be be)
{
    cint32 tmp; memcpy (&tmp, &be, sizeof tmp); return C_INT32_FROM_BE (tmp);
}

static inline cuint32 cuint32_from_be (const cuint32_be be)
{
    cuint32 tmp; memcpy (&tmp, &be, sizeof tmp); return C_UINT32_FROM_BE (tmp);
}

struct tzhead
{
    cchar           tzhMagic[4];
    cchar           tzhVersion;
    cuchar          tzhReserved[15];

    cuint32_be      tzhTtisgmtcnt;
    cuint32_be      tzhTtisstdcnt;
    cuint32_be      tzhLeapcnt;
    cuint32_be      tzhTimecnt;
    cuint32_be      tzhTypecnt;
    cuint32_be      tzhCharcnt;
};

struct ttinfo
{
    cint32_be       ttGmtOff;
    cuint8          ttIsDst;
    cuint8          ttAbBrind;
};

/**
 * A Transition Date structure for TZ Rules, an intermediate structure
 * for parsing MSWindows and Environment-variable time zones. It
 * Generalizes MSWindows's SYSTEMTIME struct.
 */
typedef struct
{
    cint            year;
    cint            mon;
    cint            mday;
    cint            wday;
    cint            week;
    cint32          offset;  /* hour*3600 + min*60 + sec; can be negative.  */
} TimeZoneDate;

#define NAME_SIZE 33

/**
 * A MSWindows-style time zone transition rule. Generalizes the
 * MSWindows TIME_ZONE_INFORMATION struct. Also used to compose time
 * zones from tzset-style identifiers.
 */
typedef struct
{
    cuint           startYear;
    cint32          stdOffset;
    cint32          dltOffset;
    TimeZoneDate    dltStart;
    TimeZoneDate    dltEnd;
    cchar           stdName[NAME_SIZE];
    cchar           dltName[NAME_SIZE];
} TimeZoneRule;

/**
 * GTimeZone's internal representation of a Daylight Savings (Summer)
 * time interval.
 */
typedef struct
{
    cint32      gmtOffset;
    bool        isDst;
    cchar*      abbrev;
} TransitionInfo;

/**
 * GTimeZone's representation of a transition time to or from Daylight
 * Savings (Summer) time and Standard time for the zone. */
typedef struct
{
    cint64      time;
    cint        infoIndex;
} Transition;

/* GTimeZone structure */
struct _CTimeZone
{
    cchar*      name;
    CArray*     tInfo;         /* Array of TransitionInfo */
    CArray*     transitions;    /* Array of Transition */
    cint        refCount;
};

C_LOCK_DEFINE_STATIC (gsTimeZones);
static CHashTable*  gsTimeZones;         // <string?, GTimeZone>
C_LOCK_DEFINE_STATIC (gsTzDefault);
static CTimeZone*   gsTzDefault = NULL;
C_LOCK_DEFINE_STATIC (gsTzLocal);
static CTimeZone*   gsTzLocal = NULL;

#define MIN_TZYEAR 1916                 // Daylight Savings started in WWI
#define MAX_TZYEAR 2999                 // And it's not likely ever to go away, but there's no point in getting carried away.


static char* zone_identifier_unix (void);
static const char* zone_info_base_dir (void);
static void find_relative_date (TimeZoneDate* buffer);
static bool parse_offset (char** pos, cint32* target);
static CTimeZone* parse_footertz (const char*, size_t);
static bool interval_valid (CTimeZone* tz, cuint interval);
static bool set_tz_name (char** pos, char* buffer, cuint size);
static bool parse_mwd_boundary (char** pos, TimeZoneDate* boundary);
static bool parse_identifier_boundaries (char** pos, TimeZoneRule *tzr);
static void zone_for_constant_offset (CTimeZone* gtz, const char* name);
static bool parse_identifier_boundary (char** pos, TimeZoneDate* target);
static bool parse_time (const char* time_, cint32* offset, bool rfc8536);
static bool parse_tz_boundary (const char* identifier, TimeZoneDate* boundary);
static cuint create_ruleset_from_rule (TimeZoneRule** rules, TimeZoneRule* rule);
static cuint rules_from_identifier (const char* identifier, TimeZoneRule** rules);
static bool parse_constant_offset (const char* name, cint32* offset, bool rfc8536);
static cint64 boundary_for_year (TimeZoneDate* boundary, cint year, cint32 offset);
static CBytes* zone_info_unix (const char* identifier, const char* resolvedIdentifier);
static bool parse_julian_boundary (char** pos, TimeZoneDate* boundary, bool ignoreLeap);
static void init_zone_from_iana_info (CTimeZone* gtz, CBytes* zoneInfo, char* identifier);
static void fill_transition_info_from_rule (TransitionInfo* info, TimeZoneRule* rule, bool isDst);
static void init_zone_from_rules (CTimeZone* gtz, TimeZoneRule* rules, cuint rulesNum, char* identifier);


inline static const TransitionInfo* interval_info (CTimeZone* tz, cuint interval)
{
    cuint index;
    c_return_val_if_fail (tz->tInfo != NULL, NULL);
    if (interval && tz->transitions && interval <= tz->transitions->len) {
        index = (TRANSITION(interval - 1)).infoIndex;
    }
    else {
        for (index = 0; index < tz->tInfo->len; index++) {
            TransitionInfo *tzinfo = &(TRANSITION_INFO(index));
            if (!tzinfo->isDst) {
                return tzinfo;
            }
        }
        index = 0;
    }

    return &(TRANSITION_INFO(index));
}

inline static cint64 interval_start (CTimeZone* tz, cuint interval)
{
    if (!interval || tz->transitions == NULL || tz->transitions->len == 0) {
        return C_MIN_INT64;
    }
    if (interval > tz->transitions->len) {
        interval = tz->transitions->len;
    }

    return (TRANSITION(interval - 1)).time;
}

inline static cint64 interval_end (CTimeZone* tz, cuint interval)
{
    if (tz->transitions && interval < tz->transitions->len) {
        cint64 lim = (TRANSITION(interval)).time;
        return lim - (lim != C_MIN_INT64);
    }
    return C_MAX_INT64;
}

inline static cint32 interval_offset (CTimeZone* tz, cuint interval)
{
    c_return_val_if_fail (tz->tInfo != NULL, 0);
    return interval_info (tz, interval)->gmtOffset;
}

inline static bool interval_isdst (CTimeZone* tz, cuint interval)
{
    c_return_val_if_fail (tz->tInfo != NULL, 0);
    return interval_info (tz, interval)->isDst;
}


inline static char* interval_abbrev (CTimeZone* tz, cuint interval)
{
    c_return_val_if_fail (tz->tInfo != NULL, 0);
    return interval_info (tz, interval)->abbrev;
}

inline static cint64 interval_local_start (CTimeZone* tz, cuint interval)
{
    if (interval) {
        return interval_start (tz, interval) + interval_offset (tz, interval);
    }

    return C_MIN_INT64;
}

inline static cint64 interval_local_end (CTimeZone* tz, cuint interval)
{
    if (tz->transitions && interval < tz->transitions->len) {
        return interval_end (tz, interval) + interval_offset (tz, interval);
    }

    return C_MAX_INT64;
}


CTimeZone* c_time_zone_new (const char* identifier)
{
    CTimeZone *tz = c_time_zone_new_identifier (identifier);

    /* Always fall back to UTC. */
    if (tz == NULL) {
        tz = c_time_zone_new_utc ();
    }

    c_assert (tz != NULL);

    return c_steal_pointer (&tz);
}

CTimeZone* c_time_zone_new_identifier (const char* identifier)
{
    CTimeZone* tz = NULL;
    TimeZoneRule* rules;
    cint rulesNum;
    char* resolvedIdentifier = NULL;

    if (identifier) {
        C_LOCK (gsTimeZones);
        if (gsTimeZones == NULL) {
            gsTimeZones = c_hash_table_new (c_str_hash, c_str_equal);
        }

        tz = c_hash_table_lookup (gsTimeZones, identifier);
        if (tz) {
            c_atomic_int_inc (&tz->refCount);
            C_UNLOCK (gsTimeZones);
            return tz;
        }
        else {
            resolvedIdentifier = c_strdup (identifier);
        }
    }
    else {
        C_LOCK (gsTzDefault);
        resolvedIdentifier = zone_identifier_unix ();
        if (gsTzDefault) {
            /* Flush default if changed. If the identifier couldn’t be resolved,
             * we’re going to fall back to UTC eventually, so don’t clear out the
             * cache if it’s already UTC. */
            if (!(resolvedIdentifier == NULL && c_str_equal (gsTzDefault->name, "UTC")) && c_strcmp0 (gsTzDefault->name, resolvedIdentifier) != 0) {
                c_clear_pointer ((void**) &gsTzDefault, (CDestroyNotify) c_time_zone_unref);
            }
            else {
                tz = c_time_zone_ref (gsTzDefault);
                C_UNLOCK (gsTzDefault);
                c_free (resolvedIdentifier);
                return tz;
            }
        }
    }

    tz = c_malloc0(sizeof(CTimeZone));
    tz->refCount = 0;

    zone_for_constant_offset (tz, identifier);

    if (tz->tInfo == NULL && (rulesNum = (int) rules_from_identifier (identifier, &rules))) {
        init_zone_from_rules (tz, rules, rulesNum, c_steal_pointer (&resolvedIdentifier));
        c_free (rules);
    }

    if (tz->tInfo == NULL) {
        CBytes* zoneInfo = zone_info_unix (identifier, resolvedIdentifier);
        if (zoneInfo != NULL) {
            init_zone_from_iana_info (tz, zoneInfo, c_steal_pointer (&resolvedIdentifier));
            c_bytes_unref (zoneInfo);
        }
    }

    c_free (resolvedIdentifier);

    /* Failed to load the timezone. */
    if (tz->tInfo == NULL) {
        c_free(tz);
        if (identifier) {
            C_UNLOCK (gsTimeZones);
        }
        else {
            C_UNLOCK (gsTzDefault);
        }

        return NULL;
    }

    c_assert (tz->name != NULL);
    c_assert (tz->tInfo != NULL);

    if (identifier) {
        c_hash_table_insert (gsTimeZones, tz->name, tz);
    }
    else if (tz->name) {
        /* Caching reference */
        c_atomic_int_inc (&tz->refCount);
        gsTzDefault = tz;
    }

    c_atomic_int_inc (&tz->refCount);

    if (identifier) {
        C_UNLOCK (gsTimeZones);
    }
    else {
        C_UNLOCK (gsTzDefault);
    }

    return tz;
}

CTimeZone* c_time_zone_new_utc (void)
{
    static CTimeZone* utc = NULL;
    static csize initialised;

    if (c_once_init_enter (&initialised)) {
        utc = c_time_zone_new_identifier ("UTC");
        c_assert (utc != NULL);
        c_once_init_leave (&initialised, true);
    }

    return c_time_zone_ref (utc);
}

CTimeZone* c_time_zone_new_local (void)
{
    const char* tzenv = c_getenv ("TZ");
    CTimeZone *tz;

    C_LOCK (gsTzLocal);

    /* Is time zone changed and must be flushed? */
    if (gsTzLocal && c_strcmp0 (c_time_zone_get_identifier (gsTzLocal), tzenv)) {
        c_clear_pointer ((void**) &gsTzLocal, (CDestroyNotify) c_time_zone_unref);
    }

    if (gsTzLocal == NULL) {
        gsTzLocal = c_time_zone_new_identifier (tzenv);
    }

    if (gsTzLocal == NULL) {
        gsTzLocal = c_time_zone_new_utc ();
    }

    tz = c_time_zone_ref (gsTzLocal);

    C_UNLOCK (gsTzLocal);

    return tz;
}

CTimeZone* c_time_zone_new_offset (cint32 seconds)
{
    char* identifier = NULL;

    identifier = c_strdup_printf ("%c%02u:%02u:%02u",
                                  (seconds >= 0) ? '+' : '-',
                                  (C_ABS (seconds) / 60) / 60,
                                  (C_ABS (seconds) / 60) % 60,
                                  C_ABS (seconds) % 60);
    CTimeZone* tz = c_time_zone_new_identifier (identifier);

    if (tz == NULL) {
        tz = c_time_zone_new_utc ();
    }
    else {
        c_assert (c_time_zone_get_offset (tz, 0) == seconds);
    }

    c_assert (tz != NULL);
    c_free (identifier);

    return tz;

}

CTimeZone* c_time_zone_ref (CTimeZone* tz)
{
    c_return_val_if_fail(tz, NULL);

    c_assert (tz->refCount > 0);

    c_atomic_int_inc (&tz->refCount);

    return tz;
}

void c_time_zone_unref (CTimeZone* tz)
{
    c_return_if_fail(tz);

    int refCount;

again:
    refCount = c_atomic_int_get (&tz->refCount);

    c_assert (refCount > 0);

    if (refCount == 1) {
        if (tz->name != NULL) {
            C_LOCK(gsTimeZones);

            /* someone else might have grabbed a ref in the meantime */
            if C_UNLIKELY (c_atomic_int_get (&tz->refCount) != 1) {
                C_UNLOCK(gsTimeZones);
                goto again;
            }

            if (gsTimeZones != NULL) {
                c_hash_table_remove (gsTimeZones, tz->name);
            }
            C_UNLOCK(gsTimeZones);
        }

        if (tz->tInfo != NULL) {
            cuint idx;
            for (idx = 0; idx < tz->tInfo->len; idx++) {
                TransitionInfo *info = &c_array_index (tz->tInfo, TransitionInfo, idx);
                c_free (info->abbrev);
            }
            c_array_free (tz->tInfo, true);
        }
        if (tz->transitions != NULL) {
            c_array_free (tz->transitions, true);
        }
        c_free (tz->name);
        c_free (tz);
    }
    else if C_UNLIKELY (!c_atomic_int_compare_and_exchange (&tz->refCount, refCount, refCount - 1)) {
        goto again;
    }
}

int c_time_zone_find_interval (CTimeZone* tz, CTimeType type, cint64 time_)
{
    cuint i, intervals;
    bool intervalIsDst;

    if (tz->transitions == NULL) {
        return 0;
    }

    intervals = tz->transitions->len;
    for (i = 0; i <= intervals; i++) {
        if (time_ <= interval_end (tz, i)) {
            break;
        }
    }

    if (type == C_TIME_TYPE_UNIVERSAL) {
        return i;
    }

    if (time_ < interval_local_start (tz, i)) {
        if (time_ > interval_local_end (tz, --i)) {
            return -1;
        }
    }
    else if (time_ > interval_local_end (tz, i)) {
        if (time_ < interval_local_start (tz, ++i)) {
            return -1;
        }
    }
    else {
        intervalIsDst = interval_isdst (tz, i);
        if  ((intervalIsDst && type != C_TIME_TYPE_DAYLIGHT) || (!intervalIsDst && type == C_TIME_TYPE_DAYLIGHT)) {
            if (i && time_ <= interval_local_end (tz, i - 1)) {
                i--;
            }
            else if (i < intervals && time_ >= interval_local_start (tz, i + 1)) {
                i++;
            }
        }
    }

    return (int) i;
}

int c_time_zone_adjust_time (CTimeZone* tz, CTimeType type, cint64* time_)
{
    cuint i, intervals;
    bool intervalIsDst;

    if (tz->transitions == NULL) {
        return 0;
    }

    intervals = tz->transitions->len;

    /* find the interval containing *time UTC
     * TODO: this could be binary searched (or better) */
    for (i = 0; i <= intervals; i++) {
        if (*time_ <= interval_end (tz, i)) {
            break;
        }
    }

    c_assert (interval_start (tz, i) <= *time_ && *time_ <= interval_end (tz, i));

    if (type != C_TIME_TYPE_UNIVERSAL) {
        if (*time_ < interval_local_start (tz, i)) {
            i--;
            /* if it's not in the previous interval... */
            if (*time_ > interval_local_end (tz, i)) {
                /* it doesn't exist.  fast-forward it. */
                i++;
                *time_ = interval_local_start (tz, i);
            }
        }
        else if (*time_ > interval_local_end (tz, i)) {
            /* if time came after the end of this interval... */
            i++;
            /* if it's not in the next interval... */
            if (*time_ < interval_local_start (tz, i)) {
                /* it doesn't exist.  fast-forward it. */
                *time_ = interval_local_start (tz, i);
            }
        }
        else {
            intervalIsDst = interval_isdst (tz, i);
            if ((intervalIsDst && type != C_TIME_TYPE_DAYLIGHT) || (!intervalIsDst && type == C_TIME_TYPE_DAYLIGHT)) {
                /* it's in this interval, but dst flag doesn't match.
                 * check neighbours for a better fit. */
                if (i && *time_ <= interval_local_end (tz, i - 1)) {
                    i--;
                }
                else if (i < intervals && *time_ >= interval_local_start (tz, i + 1)) {
                    i++;
                }
            }
        }
    }

    return (int) i;
}

const char* c_time_zone_get_abbreviation (CTimeZone* tz, int interval)
{
    c_return_val_if_fail (interval_valid (tz, (cuint)interval), NULL);

    return interval_abbrev (tz, (cuint)interval);
}

cint32 c_time_zone_get_offset (CTimeZone* tz, int interval)
{
    c_return_val_if_fail (interval_valid (tz, (cuint)interval), 0);

    return interval_offset (tz, (cuint)interval);
}

bool c_time_zone_is_dst (CTimeZone* tz, int interval)
{
    c_return_val_if_fail (interval_valid (tz, interval), false);

    if (tz->transitions == NULL) {
        return false;
    }

    return interval_isdst (tz, (cuint)interval);
}

const char* c_time_zone_get_identifier (CTimeZone* tz)
{
    c_return_val_if_fail (tz, NULL);

    return tz->name;
}

static bool parse_time (const char* time_, cint32* offset, bool rfc8536)
{
    if (*time_ < '0' || '9' < *time_) {
        return false;
    }

    *offset = 60 * 60 * (*time_++ - '0');

    if (*time_ == '\0') {
        return true;
    }

    if (*time_ != ':') {
        if (*time_ < '0' || '9' < *time_) {
            return false;
        }

        *offset *= 10;
        *offset += 60 * 60 * (*time_++ - '0');

        if (rfc8536) {
            if ('0' <= *time_ && *time_ <= '9') {
                *offset *= 10;
                *offset += 60 * 60 * (*time_++ - '0');
            }
            if (*offset > 167 * 60 * 60) {
                return false;
            }
        }
        else if (*offset > 24 * 60 * 60) {
            return false;
        }

        if (*time_ == '\0') {
            return true;
        }
    }

    if (*time_ == ':') {
        time_++;
    }
    else if (rfc8536) {
        return false;
    }

    if (*time_ < '0' || '5' < *time_) {
        return false;
    }

    *offset += 10 * 60 * (*time_++ - '0');

    if (*time_ < '0' || '9' < *time_) {
        return false;
    }

    *offset += 60 * (*time_++ - '0');

    if (*time_ == '\0') {
        return true;
    }

    if (*time_ == ':') {
        time_++;
    }
    else if (rfc8536) {
        return false;
    }

    if (*time_ < '0' || '5' < *time_) {
        return false;
    }

    *offset += 10 * (*time_++ - '0');

    if (*time_ < '0' || '9' < *time_) {
        return false;
    }

    *offset += *time_++ - '0';

    return *time_ == '\0';
}

static bool parse_constant_offset (const char* name, cint32* offset, bool rfc8536)
{
    if (!rfc8536 && c_strcmp0 (name, "UTC") == 0) {
        *offset = 0;
        return true;
    }

    if (*name >= '0' && '9' >= *name) {
        return parse_time (name, offset, rfc8536);
    }

    switch (*name++) {
        case 'Z': {
            *offset = 0;
            /* Internet RFC 8536 section 3.3.1 requires a numeric zone.  */
            return !rfc8536 && !*name;
        }
        case '+': {
            return parse_time (name, offset, rfc8536);
        }
        case '-': {
            if (parse_time (name, offset, rfc8536)) {
                *offset = -*offset;
                return true;
            }
            else {
                return false;
            }
        }
        default: {
            return false;
        }
    }
}

static void zone_for_constant_offset (CTimeZone* gtz, const char* name)
{
    cint32 offset;
    TransitionInfo info;

    if (name == NULL || !parse_constant_offset (name, &offset, false)) {
        return;
    }

    info.gmtOffset = offset;
    info.isDst = false;
    info.abbrev =  c_strdup (name);
    gtz->name = c_strdup (name);
    gtz->tInfo = c_array_sized_new (false, true, sizeof (TransitionInfo), 1);
    c_array_append_val (gtz->tInfo, info);

    gtz->transitions = NULL;
}

static const char* zone_info_base_dir (void)
{
    if (c_file_test ("/usr/share/zoneinfo", C_FILE_TEST_IS_DIR)) {
        return "/usr/share/zoneinfo";     /* Most distros */
    }
    else if (c_file_test ("/usr/share/lib/zoneinfo", C_FILE_TEST_IS_DIR)) {
        return "/usr/share/lib/zoneinfo"; /* Illumos distros */
    }

    /* need a better fallback case */
    return "/usr/share/zoneinfo";
}

static char* zone_identifier_unix (void)
{
    char* resolvedIdentifier = NULL;
    csize prefixLen = 0;
    char* canonicalPath = NULL;
    CError* readLinkErr = NULL;
    const char* tzdir;
    bool notASymlinkToZoneinfo = false;
    struct stat fileStatus;

    /* Resolve the actual timezone pointed to by /etc/localtime. */
    resolvedIdentifier = c_file_read_link ("/etc/localtime", &readLinkErr);

    if (resolvedIdentifier != NULL) {
        if (!c_path_is_absolute (resolvedIdentifier)) {
            char *absoluteResolvedIdentifier = c_build_filename ("/etc", resolvedIdentifier, NULL);
            c_free (resolvedIdentifier);
            resolvedIdentifier = c_steal_pointer (&absoluteResolvedIdentifier);
        }

        if (c_lstat (resolvedIdentifier, &fileStatus) == 0) {
            if ((fileStatus.st_mode & S_IFMT) != S_IFREG) {
                c_clear_pointer ((void**)&resolvedIdentifier, c_free0);
                notASymlinkToZoneinfo = true;
            }
        }
        else {
            c_clear_pointer ((void**) &resolvedIdentifier, c_free0);
        }
    }
    else {
        notASymlinkToZoneinfo = c_error_matches (readLinkErr, C_FILE_ERROR, C_FILE_ERROR_INVAL);
        c_clear_error (&readLinkErr);
    }

    if (resolvedIdentifier == NULL) {
        if (notASymlinkToZoneinfo && (c_file_get_contents ("/var/db/zoneinfo", &resolvedIdentifier, NULL, NULL) || c_file_get_contents ("/etc/timezone", &resolvedIdentifier, NULL, NULL))) {
            c_strchomp (resolvedIdentifier);
        }
        else {
            c_assert (resolvedIdentifier == NULL);
            goto out;
        }
    }
    else {
        canonicalPath = c_canonicalize_filename (resolvedIdentifier, "/etc");
        c_free (resolvedIdentifier);
        resolvedIdentifier = c_steal_pointer (&canonicalPath);
    }

    tzdir = c_getenv ("TZDIR");
    if (tzdir == NULL) {
        tzdir = zone_info_base_dir ();
    }

    /* Strip the prefix and slashes if possible. */
    if (c_str_has_prefix (resolvedIdentifier, tzdir)) {
        prefixLen = strlen (tzdir);
        while (*(resolvedIdentifier + prefixLen) == '/') {
            prefixLen++;
        }
    }

    if (prefixLen > 0) {
        memmove (resolvedIdentifier, resolvedIdentifier + prefixLen, strlen (resolvedIdentifier) - prefixLen + 1  /* nul terminator */);
    }

    c_assert (resolvedIdentifier != NULL);

out:
    c_free (canonicalPath);

    return resolvedIdentifier;
}

static CBytes* zone_info_unix (const char* identifier, const char* resolvedIdentifier)
{
    char *filename = NULL;
    CMappedFile* file = NULL;
    CBytes* zoneInfo = NULL;
    const char* tzdir = NULL;

    tzdir = c_getenv ("TZDIR");
    if (tzdir == NULL) {
        tzdir = zone_info_base_dir ();
    }

    if (identifier != NULL) {
        if (*identifier == ':') {
            identifier ++;
        }

        if (c_path_is_absolute (identifier)) {
            filename = c_strdup (identifier);
        }
        else {
            filename = c_build_filename (tzdir, identifier, NULL);
        }
    }
    else {
        if (resolvedIdentifier == NULL) {
            goto out;
        }

        filename = c_strdup ("/etc/localtime");
    }

    file = c_mapped_file_new (filename, false, NULL);
    if (file != NULL) {
        zoneInfo = c_bytes_new_with_free_func (c_mapped_file_get_contents (file),
                            c_mapped_file_get_length (file),
                            (CDestroyNotify)c_mapped_file_unref,
                            c_mapped_file_ref (file));
        c_mapped_file_unref (file);
    }

    c_assert (resolvedIdentifier != NULL);

out:
    c_free (filename);

    return zoneInfo;
}

static void init_zone_from_iana_info (CTimeZone* gtz, CBytes* zoneInfo, char* identifier)
{
    csize size;
    cuint index;
    cuint32 timeCount, typeCount;
    cuint8 *tzTransitions, *tzTypeIndex, *tzTtinfo;
    cuint8 *tzAbbrs;
    csize timesize = sizeof (cint32);
    const void* headerData = c_bytes_get_data (zoneInfo, &size);
    const char *data = headerData;
    const struct tzhead* header = headerData;
    CTimeZone *footertz = NULL;
    cuint extraTimeCount = 0, extraTypeCount = 0;
    cint64 lastExplicitTransitionTime;

    c_return_if_fail (size >= sizeof (struct tzhead) && memcmp (header, "TZif", 4) == 0);

    /* FIXME: Handle invalid TZif files better (Issue#1088).  */

    if (header->tzhVersion >= '2') {
        /* Skip ahead to the newer 64-bit data if it's available. */
        header = (const struct tzhead *)
            (((const char*) (header + 1))
                + cuint32_from_be(header->tzhTtisgmtcnt)
                + cuint32_from_be(header->tzhTtisstdcnt)
                + 8 * cuint32_from_be(header->tzhLeapcnt)
                + 5 * cuint32_from_be(header->tzhTimecnt)
                + 6 * cuint32_from_be(header->tzhTypecnt)
                + cuint32_from_be(header->tzhCharcnt));
        timesize = sizeof (cint64);
    }
    timeCount = cuint32_from_be(header->tzhTimecnt);
    typeCount = cuint32_from_be(header->tzhTypecnt);

    if (header->tzhVersion >= '2') {
        const char* footer = (((const char*) (header + 1))
                            + cuint32_from_be(header->tzhTtisgmtcnt)
                            + cuint32_from_be(header->tzhTtisstdcnt)
                            + 12 * cuint32_from_be(header->tzhLeapcnt)
                            + 9 * timeCount
                            + 6 * typeCount
                            + cuint32_from_be(header->tzhCharcnt));
        const char *footerlast;
        size_t footerlen;
        c_return_if_fail (footer <= data + size - 2 && footer[0] == '\n');
        footerlast = memchr (footer + 1, '\n', data + size - (footer + 1));
        c_return_if_fail (footerlast);
        footerlen = footerlast + 1 - footer;
        if (footerlen != 2) {
            footertz = parse_footertz (footer, footerlen);
            c_return_if_fail (footertz);
            extraTypeCount = footertz->tInfo->len;
            extraTimeCount = footertz->transitions->len;
        }
    }

    tzTransitions = ((cuint8*) (header) + sizeof (*header));
    tzTypeIndex = tzTransitions + timesize * timeCount;
    tzTtinfo = tzTypeIndex + timeCount;
    tzAbbrs = tzTtinfo + sizeof (struct ttinfo) * typeCount;

    gtz->name = c_steal_pointer (&identifier);
    gtz->tInfo = c_array_sized_new (false, true, sizeof (TransitionInfo), typeCount + extraTypeCount);
    gtz->transitions = c_array_sized_new (false, true, sizeof (Transition), timeCount + extraTimeCount);

    for (index = 0; index < typeCount; index++) {
        TransitionInfo tInfo;
        struct ttinfo info = ((struct ttinfo*) tzTtinfo)[index];
        tInfo.gmtOffset = cint32_from_be (info.ttGmtOff);
        tInfo.isDst = info.ttIsDst ? true : false;
        tInfo.abbrev = c_strdup ((char*) &tzAbbrs[info.ttAbBrind]);
        c_array_append_val (gtz->tInfo, tInfo);
    }

    for (index = 0; index < timeCount; index++) {
        Transition trans;
        if (header->tzhVersion >= '2') {
            trans.time = cint64_from_be (((cint64_be*)tzTransitions)[index]);
        }
        else {
            trans.time = cint32_from_be (((cint32_be*)tzTransitions)[index]);
        }
        lastExplicitTransitionTime = trans.time;
        trans.infoIndex = tzTypeIndex[index];
        c_assert (trans.infoIndex >= 0);
        c_assert ((cuint) trans.infoIndex < gtz->tInfo->len);
        c_array_append_val (gtz->transitions, trans);
    }

    if (footertz) {
        for (index = 0; index < extraTypeCount; index++) {
            TransitionInfo tInfo;
            TransitionInfo *footerTInfo = &c_array_index (footertz->tInfo, TransitionInfo, index);
            tInfo.gmtOffset = footerTInfo->gmtOffset;
            tInfo.isDst = footerTInfo->isDst;
            tInfo.abbrev = c_steal_pointer (&footerTInfo->abbrev);
            c_array_append_val (gtz->tInfo, tInfo);
        }

        for (index = 0; index < extraTimeCount; index++) {
            Transition* footerTransition = &c_array_index (footertz->transitions, Transition, index);
            if (timeCount <= 0 || lastExplicitTransitionTime < footerTransition->time) {
                Transition trans;
                trans.time = footerTransition->time;
                trans.infoIndex = typeCount + footerTransition->infoIndex;
                c_array_append_val (gtz->transitions, trans);
            }
        }

        c_time_zone_unref (footertz);
    }
}

static void find_relative_date (TimeZoneDate* buffer)
{
    cuint wday;
    CDate date;
    c_date_clear (&date, 1);
    wday = buffer->wday;

    if (buffer->mon == 13 || buffer->mon == 14) {
        c_date_set_dmy (&date, 1, 1, buffer->year);
        if (wday >= 59 && buffer->mon == 13 && c_date_is_leap_year (buffer->year)) {
            c_date_add_days (&date, wday);
        }
        else {
            c_date_add_days (&date, wday - 1);
        }
        buffer->mon = (int) c_date_get_month (&date);
        buffer->mday = (int) c_date_get_day (&date);
        buffer->wday = 0;
    }
    else {
        cuint days;
        cuint daysInMonth = c_date_get_days_in_month (buffer->mon, buffer->year);
        CDateWeekday firstWday;

        c_date_set_dmy (&date, 1, buffer->mon, buffer->year);
        firstWday = c_date_get_weekday (&date);

        if ((cuint) firstWday > wday) {
            ++(buffer->week);
        }
        /* week is 1 <= w <= 5, we need 0-based */
        days = 7 * (buffer->week - 1) + wday - firstWday;

        /* "days" is a 0-based offset from the 1st of the month.
         * Adding days == days_in_month would bring us into the next month,
         * hence the ">=" instead of just ">".
         */
        while (days >= daysInMonth) {
            days -= 7;
        }

        c_date_add_days (&date, days);

        buffer->mday = c_date_get_day (&date);
    }
}

static cint64 boundary_for_year (TimeZoneDate* boundary, cint year, cint32 offset)
{
    TimeZoneDate buffer;
    CDate date;
    const cuint64 unixEpochStart = 719163L;
    const cuint64 secondsPerDay = 86400L;

    if (!boundary->mon) {
        return 0;
    }

    buffer = *boundary;

    if (boundary->year == 0) {
        buffer.year = year;
        if (buffer.wday) {
            find_relative_date (&buffer);
        }
    }

    c_assert (buffer.year == year);
    c_date_clear (&date, 1);
    c_date_set_dmy (&date, buffer.mday, buffer.mon, buffer.year);
    return (cint64) ((c_date_get_julian (&date) - unixEpochStart) * secondsPerDay + buffer.offset - offset);
}

static void fill_transition_info_from_rule (TransitionInfo* info, TimeZoneRule* rule, bool isDst)
{
    cint offset = isDst ? rule->dltOffset : rule->stdOffset;
    char* name = isDst ? rule->dltName : rule->stdName;

    info->gmtOffset = offset;
    info->isDst = isDst;

    if (name) {
        info->abbrev = c_strdup (name);
    }
    else {
        info->abbrev = c_strdup_printf ("%+03d%02d", (int) offset / 3600, (int) abs (offset / 60) % 60);
    }
}

static void init_zone_from_rules (CTimeZone* gtz, TimeZoneRule* rules, cuint rulesNum, char* identifier)
{
    cuint typeCount = 0, transCount = 0, infoIndex = 0;
    cuint ri; /* rule index */
    bool skipFirstStdTrans = true;
    cint32 lastOffset;

    typeCount = 0;
    transCount = 0;

    for (ri = 0; ri < rulesNum - 1; ri++) {
        if (rules[ri].dltStart.mon || rules[ri].dltEnd.mon) {
            cuint rulespan = (rules[ri + 1].startYear - rules[ri].startYear);
            cuint transitions = rules[ri].dltStart.mon > 0 ? 1 : 0;
            transitions += rules[ri].dltEnd.mon > 0 ? 1 : 0;
            typeCount += rules[ri].dltStart.mon > 0 ? 2 : 1;
            transCount += transitions * rulespan;
        }
        else {
            typeCount++;
        }
    }

    gtz->name = c_steal_pointer (&identifier);
    gtz->tInfo = c_array_sized_new (false, true, sizeof (TransitionInfo), typeCount);
    gtz->transitions = c_array_sized_new (false, true, sizeof (Transition), transCount);

    lastOffset = rules[0].stdOffset;

    for (ri = 0; ri < rulesNum - 1; ri++) {
        if ((rules[ri].stdOffset || rules[ri].dltOffset)
            && rules[ri].dltStart.mon == 0 && rules[ri].dltEnd.mon == 0) {
            TransitionInfo stdInfo;
            fill_transition_info_from_rule (&stdInfo, &(rules[ri]), false);
            c_array_append_val (gtz->tInfo, stdInfo);

            if (ri > 0 && ((rules[ri - 1].dltStart.mon > 12 && rules[ri - 1].dltStart.wday > rules[ri - 1].dltEnd.wday) || rules[ri - 1].dltStart.mon > rules[ri - 1].dltEnd.mon)) {
                cuint year = rules[ri].startYear;
                cint64 std_time =  boundary_for_year (&rules[ri].dltEnd, (cint) year, lastOffset);
                Transition std_trans = {std_time, (cint) infoIndex};
                c_array_append_val (gtz->transitions, std_trans);
            }
            lastOffset = rules[ri].stdOffset;
            ++infoIndex;
            skipFirstStdTrans = true;
        }
        else {
            const cuint startYear = rules[ri].startYear;
            const cuint endYear = rules[ri + 1].startYear;
            bool dltFirst;
            cuint year;
            TransitionInfo stdInfo, dltInfo;
            if (rules[ri].dltStart.mon > 12) {
                dltFirst = rules[ri].dltStart.wday > rules[ri].dltEnd.wday;
            }
            else {
                dltFirst = rules[ri].dltStart.mon > rules[ri].dltEnd.mon;
            }
            fill_transition_info_from_rule (&stdInfo, &(rules[ri]), false);
            fill_transition_info_from_rule (&dltInfo, &(rules[ri]), true);

            c_array_append_val (gtz->tInfo, stdInfo);
            c_array_append_val (gtz->tInfo, dltInfo);

            for (year = startYear; year < endYear; year++) {
                cint32 dltOffset = (dltFirst ? lastOffset : rules[ri].dltOffset);
                cint32 stdOffset = (dltFirst ? rules[ri].stdOffset : lastOffset);
                cint64 stdTime = boundary_for_year (&rules[ri].dltEnd, (cint) year, dltOffset);
                cint64 dltTime = boundary_for_year (&rules[ri].dltStart, (cint) year, stdOffset);
                Transition stdTrans = {stdTime, (cint) infoIndex};
                Transition dltTrans = {dltTime, (cint) infoIndex + 1};
                lastOffset = (dltFirst ? rules[ri].dltOffset : rules[ri].stdOffset);
                if (dltFirst) {
                    if (skipFirstStdTrans) {
                        skipFirstStdTrans = false;
                    }
                    else if (stdTime) {
                        c_array_append_val (gtz->transitions, stdTrans);
                    }
                    if (dltTime) {
                        c_array_append_val (gtz->transitions, dltTrans);
                    }
                }
                else {
                    if (dltTime) {
                        c_array_append_val (gtz->transitions, dltTrans);
                    }
                    if (stdTime) {
                        c_array_append_val (gtz->transitions, stdTrans);
                    }
                }
            }

            infoIndex += 2;
        }
    }
    if (ri > 0 && ((rules[ri - 1].dltStart.mon > 12 && rules[ri - 1].dltStart.wday > rules[ri - 1].dltEnd.wday) || rules[ri - 1].dltStart.mon > rules[ri - 1].dltEnd.mon)) {
        TransitionInfo info;
        cuint year = rules[ri].startYear;
        Transition trans;
        fill_transition_info_from_rule (&info, &(rules[ri - 1]), false);
        c_array_append_val (gtz->tInfo, info);
        trans.time = boundary_for_year (&rules[ri - 1].dltEnd, (cint) year, lastOffset);
        trans.infoIndex = (cint) infoIndex;
        c_array_append_val (gtz->transitions, trans);
    }
}

/**
 * parses date[/time] for parsing TZ environment variable
 *
 * date is either Mm.w.d, Jn or N
 * - m is 1 to 12
 * - w is 1 to 5
 * - d is 0 to 6
 * - n is 1 to 365
 * - N is 0 to 365
 *
 * time is either h or hh[[:]mm[[[:]ss]]]
 *  - h[h] is 0 to 24
 *  - mm is 00 to 59
 *  - ss is 00 to 59
 */
static bool parse_mwd_boundary (char** pos, TimeZoneDate* boundary)
{
    c_return_val_if_fail(pos, false);
    c_return_val_if_fail(*pos, false);

    cint month, week, day;

    if (**pos == '\0' || **pos < '0' || '9' < **pos) {
        return false;
    }

    month = *(*pos)++ - '0';

    if ((month == 1 && **pos >= '0' && '2' >= **pos) || (month == 0 && **pos >= '0' && '9' >= **pos)) {
        month *= 10;
        month += *(*pos)++ - '0';
    }

    if (*(*pos)++ != '.' || month == 0) {
        return false;
    }

    if (**pos == '\0' || **pos < '1' || '5' < **pos) {
        return false;
    }

    week = *(*pos)++ - '0';

    if (*(*pos)++ != '.') {
        return false;
    }

    if (**pos == '\0' || **pos < '0' || '6' < **pos) {
        return false;
    }

    day = *(*pos)++ - '0';

    if (!day) {
        day += 7;
    }

    boundary->year = 0;
    boundary->mon = month;
    boundary->week = week;
    boundary->wday = day;

    return true;
}

static bool parse_julian_boundary (char** pos, TimeZoneDate* boundary, bool ignoreLeap)
{
    cint day = 0;
    CDate date;

    while (**pos >= '0' && '9' >= **pos) {
        day *= 10;
        day += *(*pos)++ - '0';
    }

    if (ignoreLeap) {
        if (day < 1 || 365 < day) {
            return false;
        }
        if (day >= 59) {
            day++;
        }
    }
    else {
        if (day < 0 || 365 < day) {
            return false;
        }
        day++;
    }

    c_date_clear (&date, 1);
    c_date_set_julian (&date, day);
    boundary->year = 0;
    boundary->mon = (int) c_date_get_month (&date);
    boundary->mday = (int) c_date_get_day (&date);
    boundary->wday = 0;

    return true;
}

static bool parse_tz_boundary (const char* identifier, TimeZoneDate* boundary)
{
    char *pos = (char*)identifier;
    if (*pos == 'M') {
        ++pos;
        if (!parse_mwd_boundary (&pos, boundary)) {
            return false;
        }
    }
        /* Julian date which ignores Feb 29 in leap years */
    else if (*pos == 'J') {
        ++pos;
        if (!parse_julian_boundary (&pos, boundary, true)) {
            return false;
        }
    }
        /* Julian date which counts Feb 29 in leap years */
    else if (*pos >= '0' && '9' >= *pos) {
        if (!parse_julian_boundary (&pos, boundary, false)) {
            return false;
        }
    }
    else {
        return false;
    }

    /* Time */
    if (*pos == '/') {
        return parse_constant_offset (pos + 1, &boundary->offset, true);
    }
    else {
        boundary->offset = 2 * 60 * 60;
        return *pos == '\0';
    }
}

static cuint create_ruleset_from_rule (TimeZoneRule** rules, TimeZoneRule* rule)
{
    *rules = c_malloc0(sizeof(TimeZoneRule) * 2);

    (*rules)[0].startYear = MIN_TZYEAR;
    (*rules)[1].startYear = MAX_TZYEAR;

    (*rules)[0].stdOffset = -rule->stdOffset;
    (*rules)[0].dltOffset = -rule->dltOffset;
    (*rules)[0].dltStart  = rule->dltStart;
    (*rules)[0].dltEnd = rule->dltEnd;
    strcpy ((*rules)[0].stdName, rule->stdName);
    strcpy ((*rules)[0].dltName, rule->dltName);

    return 2;
}

static bool parse_offset (char** pos, cint32* target)
{
    char* buffer;
    char* targetPos = *pos;
    bool ret;

    while (**pos == '+' || **pos == '-' || **pos == ':' || (**pos >= '0' && '9' >= **pos)) {
        ++(*pos);
    }

    buffer = c_strndup (targetPos, *pos - targetPos);
    ret = parse_constant_offset (buffer, target, false);
    c_free (buffer);

    return ret;
}

static bool parse_identifier_boundary (char** pos, TimeZoneDate* target)
{
    char* buffer;
    char* targetPos = *pos;
    bool ret;

    while (**pos != ',' && **pos != '\0') {
        ++(*pos);
    }
    buffer = c_strndup (targetPos, *pos - targetPos);
    ret = parse_tz_boundary (buffer, target);
    c_free (buffer);

    return ret;
}

static bool set_tz_name (char** pos, char* buffer, cuint size)
{
    bool quoted = **pos == '<';
    char *namePos = *pos;
    cuint len;

    c_assert (size != 0);

    if (quoted) {
        namePos++;
        do {
            ++(*pos);
        }
        while (c_ascii_isalnum (**pos) || **pos == '-' || **pos == '+');
        if (**pos != '>') {
            return false;
        }
    }
    else {
        while (c_ascii_isalpha (**pos)) {
            ++(*pos);
        }
    }

    if (*pos - namePos < 3) {
        return false;
    }

    memset (buffer, 0, size);
    len = (cuint) (*pos - namePos) > size - 1 ? size - 1 : (cuint) (*pos - namePos);
    strncpy (buffer, namePos, len);
    *pos += quoted;

    return true;
}

static bool parse_identifier_boundaries (char** pos, TimeZoneRule *tzr)
{
    if (*(*pos)++ != ',') {
        return false;
    }

    /* Start date */
    if (!parse_identifier_boundary (pos, &(tzr->dltStart)) || *(*pos)++ != ',') {
        return false;
    }

    /* End date */
    if (!parse_identifier_boundary (pos, &(tzr->dltEnd))) {
        return false;
    }

    return true;
}

static cuint rules_from_identifier (const char* identifier, TimeZoneRule** rules)
{
    char *pos;
    TimeZoneRule tzr;

    c_assert (rules != NULL);

    *rules = NULL;

    if (!identifier) {
        return 0;
    }

    pos = (char*)identifier;
    memset (&tzr, 0, sizeof (tzr));
    /* Standard offset */
    if (!(set_tz_name (&pos, tzr.stdName, NAME_SIZE)) || !parse_offset (&pos, &(tzr.stdOffset))) {
        return 0;
    }

    if (*pos == 0) {
        return create_ruleset_from_rule (rules, &tzr);
    }

    /* Format 2 */
    if (!(set_tz_name (&pos, tzr.dltName, NAME_SIZE))) {
        return 0;
    }
    parse_offset (&pos, &(tzr.dltOffset));
    if (tzr.dltOffset == 0) { /* No daylight offset given, assume it's 1 hour earlier that standard */
        tzr.dltOffset = tzr.stdOffset - 3600;
    }
    if (*pos == '\0') {
        return 0;
    }

    /* Start and end required (format 2) */
    if (!parse_identifier_boundaries (&pos, &tzr)) {
        return 0;
    }

    return create_ruleset_from_rule (rules, &tzr);
}

static CTimeZone* parse_footertz (const char* footer, size_t footerlen)
{
    char* tzstring = c_strndup (footer + 1, footerlen - 2);
    CTimeZone* footertz = NULL;

    /* FIXME: The allocation for tzstring could be avoided by
       passing a gsize identifier_len argument to rules_from_identifier
       and changing the code in that function to stop assuming that
       identifier is nul-terminated.  */
    TimeZoneRule *rules;
    cuint rulesNum = rules_from_identifier (tzstring, &rules);

    c_free (tzstring);
    if (rulesNum > 1) {
        footertz = c_malloc0(sizeof (CTimeZone));
        init_zone_from_rules (footertz, rules, rulesNum, NULL);
        footertz->refCount++;
    }
    c_free (rules);

    return footertz;
}

static bool interval_valid (CTimeZone* tz, cuint interval)
{
    if ( tz->transitions == NULL) {
        return interval == 0;
    }

    return interval <= tz->transitions->len;
}
