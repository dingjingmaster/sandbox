
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-24.
//

#include <locale.h>
#include "date.h"

#include "clib.h"

#define NUM_LEN 10

struct _CDateParseTokens
{
    int     numInts;
    int     n[3];
    cuint   month;
};
typedef struct _CDateParseTokens CDateParseTokens;

static cuint convert_twodigit_year (cuint y);
static void c_date_update_dmy (const CDate *constD);
static void c_date_update_julian (const CDate *constD);
static void c_date_prepare_to_parse (const char* str, CDateParseTokens *pt);
static void c_date_fill_parse_tokens (const char *str, CDateParseTokens *pt);
static inline bool update_month_match (csize *longest, const char *haystack, const char *needle);

C_LOCK_DEFINE_STATIC(gsDateLocker);

static cint gsLocaleEraAdjust = 0;
static bool gsUsingTwoDigitYears = false;
static const CDateYear gsTwoDigitStartYear = 1930;

static char* gsCurrentLocale = NULL;

static char* gsLongMonthNames[13]               = { NULL, };
static char* gsLongMonthNamesAlternative[13]    = { NULL, };
static char* gsShortMonthNames[13]              = { NULL, };
static char* gsShortMonthNamesAlternative[13]   = { NULL, };

static CDateDMY gsDmyOrder[3] = { C_DATE_DAY, C_DATE_MONTH, C_DATE_YEAR };

static const cuint8 gsDaysInMonths[2][13] =
    {  /* error, jan feb mar apr may jun jul aug sep oct nov dec */
        {  0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
        {  0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } /* leap year */
    };

static const cuint16 gsDaysInYear[2][14] =
    {  /* 0, jan feb mar apr may  jun  jul  aug  sep  oct  nov  dec */
        {  0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
        {  0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
    };


CDate *c_date_new(void)
{
    /* happily, 0 is the invalid flag for everything. */
    CDate *d = c_malloc0(sizeof(CDate) * 1);

    return d;
}

CDate *c_date_new_dmy(CDateDay day, CDateMonth m, CDateYear y)
{
    CDate *d;
    c_return_val_if_fail (c_date_valid_dmy (day, m, y), NULL);

    d = c_malloc0(sizeof(CDate));

    d->julian = false;
    d->dmy    = true;

    d->month = m;
    d->day   = day;
    d->year  = y;

    c_assert (c_date_valid (d));

    return d;
}

CDate *c_date_new_julian(cuint32 julianDay)
{
    CDate *d;
    c_return_val_if_fail (c_date_valid_julian (julianDay), NULL);

    d = c_malloc0(sizeof(CDate));

    d->julian = true;
    d->dmy    = false;

    d->julian_days = julianDay;

    c_assert (c_date_valid (d));

    return d;
}

void c_date_free(CDate *date)
{
    c_return_if_fail (date != NULL);

    c_free (date);
}

CDate *c_date_copy(const CDate *date)
{
    CDate *res;
    c_return_val_if_fail (date != NULL, NULL);

    if (c_date_valid (date)) {
        res = c_date_new_julian (c_date_get_julian (date));
    }
    else {
        res = c_date_new ();
        *res = *date;
    }

    return res;
}

bool c_date_valid_year(CDateYear y)
{
    return (y > C_DATE_BAD_YEAR);
}

bool c_date_valid(const CDate *d)
{
    c_return_val_if_fail (d != NULL, false);

    return (d->julian || d->dmy);
}

bool c_date_valid_month(CDateMonth m)
{
    return (((cint) m > C_DATE_BAD_MONTH) && ((cint) m < 13));
}

bool c_date_valid_day(CDateDay d)
{
    return ( (d > C_DATE_BAD_DAY) && (d < 32) );
}

bool c_date_valid_weekday(CDateWeekday weekday)
{
    return (((cint) weekday > C_DATE_BAD_WEEKDAY) && ((cint) weekday < 8));
}

bool c_date_valid_julian(cuint32 julianDate)
{
    return (julianDate > C_DATE_BAD_JULIAN);
}

bool c_date_valid_dmy(CDateDay d, CDateMonth m, CDateYear y)
{
    return ( (m > C_DATE_BAD_MONTH) &&
             (m < 13)               &&
             (d > C_DATE_BAD_DAY)   &&
             (y > C_DATE_BAD_YEAR)  &&
             (d <=  (c_date_is_leap_year (y) ?
                     gsDaysInMonths[1][m] : gsDaysInMonths[0][m])) );
}

CDateWeekday c_date_get_weekday(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), C_DATE_BAD_WEEKDAY);

    if (!d->julian)
        c_date_update_julian (d);

    c_return_val_if_fail (d->julian, C_DATE_BAD_WEEKDAY);

    return ((d->julian_days - 1) % 7) + 1;
}

CDateMonth c_date_get_month(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), C_DATE_BAD_MONTH);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, C_DATE_BAD_MONTH);

    return d->month;
}

CDateYear c_date_get_year(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), C_DATE_BAD_YEAR);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, C_DATE_BAD_YEAR);

    return d->year;
}

CDateDay c_date_get_day(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), C_DATE_BAD_DAY);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, C_DATE_BAD_DAY);

    return d->day;
}

cuint32 c_date_get_julian(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), C_DATE_BAD_JULIAN);

    if (!d->julian)
        c_date_update_julian (d);

    c_return_val_if_fail (d->julian, C_DATE_BAD_JULIAN);

    return d->julian_days;
}

cuint c_date_get_day_of_year(const CDate *d)
{
    cint idx;

    c_return_val_if_fail (c_date_valid (d), 0);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, 0);

    idx = c_date_is_leap_year (d->year) ? 1 : 0;

    return (gsDaysInYear[idx][d->month] + d->day);
}

cuint c_date_get_monday_week_of_year(const CDate *d)
{
    CDateWeekday wd;
    cuint day;
    CDate first;

    c_return_val_if_fail (c_date_valid (d), 0);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, 0);

    c_date_clear (&first, 1);

    c_date_set_dmy (&first, 1, 1, d->year);

    wd = c_date_get_weekday (&first) - 1; /* make Monday day 0 */
    day = c_date_get_day_of_year (d) - 1;

    return ((day + wd)/7U + (wd == 0 ? 1 : 0));
}

cuint c_date_get_sunday_week_of_year(const CDate *d)
{
    CDateWeekday wd;
    cuint day;
    CDate first;

    c_return_val_if_fail (c_date_valid (d), 0);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, 0);

    c_date_clear (&first, 1);

    c_date_set_dmy (&first, 1, 1, d->year);

    wd = c_date_get_weekday (&first);
    if (wd == 7) wd = 0; /* make Sunday day 0 */
    day = c_date_get_day_of_year (d) - 1;

    return ((day + wd)/7U + (wd == 0 ? 1 : 0));
}

cuint c_date_get_iso8601_week_of_year(const CDate *d)
{
    cuint j, d4, L, d1, w;

    c_return_val_if_fail (c_date_valid (d), 0);

    if (!d->julian)
        c_date_update_julian (d);

    c_return_val_if_fail (d->julian, 0);

    j  = d->julian_days + 1721425;
    d4 = (j + 31741 - (j % 7)) % 146097 % 36524 % 1461;
    L  = d4 / 1460;
    d1 = ((d4 - L) % 365) + L;
    w  = d1 / 7 + 1;

    return w;
}

cint c_date_days_between(const CDate *d1, const CDate *d2)
{
    c_return_val_if_fail (c_date_valid (d1), 0);
    c_return_val_if_fail (c_date_valid (d2), 0);

    return (cint)c_date_get_julian (d2) - (cint)c_date_get_julian (d1);
}

void c_date_clear(CDate *d, cuint nDates)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (nDates != 0);

    memset (d, 0x0, nDates * sizeof (CDate));
}

void c_date_set_parse(CDate *d, const char *str)
{
    CDateParseTokens pt;
    cuint m = C_DATE_BAD_MONTH, day = C_DATE_BAD_DAY, y = C_DATE_BAD_YEAR;
    csize str_len;

    c_return_if_fail (d != NULL);

    /* set invalid */
    c_date_clear (d, 1);

    /* Anything longer than this is ridiculous and could take a while to normalize.
     * This limit is chosen arbitrarily. */
    str_len = strlen (str);
    if (str_len > 200) {
        return;
    }

    /* The input has to be valid UTF-8. */
    if (!c_utf8_validate_len (str, str_len, NULL)) {
        return;
    }

    C_LOCK (gsDateLocker);

    c_date_prepare_to_parse (str, &pt);

    C_LOG_DEBUG_CONSOLE("Found %d ints, '%d' '%d' '%d' and written out month %d", pt.numInts, pt.n[0], pt.n[1], pt.n[2], pt.month);


    if (pt.numInts == 4) {
        C_UNLOCK (gsDateLocker);
        return; /* presumably a typo; bail out. */
    }

    if (pt.numInts > 1) {
        int i = 0;
        int j = 0;
        c_assert (pt.numInts < 4); /* i.e., it is 2 or 3 */

        while (i < pt.numInts && j < 3) {
            switch (gsDmyOrder[j]) {
                case C_DATE_MONTH: {
                    if (pt.numInts == 2 && pt.month != C_DATE_BAD_MONTH) {
                        m = pt.month;
                        ++j;      /* skip months, but don't skip this number */
                        continue;
                    }
                    else {
                        m = pt.n[i];
                    }
                }
                    break;
                case C_DATE_DAY: {
                    if (pt.numInts == 2 && pt.month == C_DATE_BAD_MONTH) {
                        day = 1;
                        ++j;      /* skip days, since we may have month/year */
                        continue;
                    }
                    day = pt.n[i];
                    break;
                }
                case C_DATE_YEAR: {
                    y  = pt.n[i];
                    if (gsLocaleEraAdjust != 0) {
                        y += gsLocaleEraAdjust;
                    }
                    y = convert_twodigit_year (y);
                    break;
                }
                default: {
                    break;
                }
            }
            ++i;
            ++j;
        }

        if (pt.numInts == 3 && !c_date_valid_dmy (day, m, y)) {
            /* Try YYYY MM DD */
            y   = pt.n[0];
            m   = pt.n[1];
            day = pt.n[2];

            if (gsUsingTwoDigitYears && y < 100) {
                y = C_DATE_BAD_YEAR; /* avoids ambiguity */
            }
        }
        else if (pt.numInts == 2) {
            if (m == C_DATE_BAD_MONTH && pt.month != C_DATE_BAD_MONTH) {
                m = pt.month;
            }
        }
    }
    else if (pt.numInts == 1) {
        if (pt.month != C_DATE_BAD_MONTH) {
            /* Month name and year? */
            m    = pt.month;
            day  = 1;
            y = pt.n[0];
        }
        else {
            /* Try yyyymmdd and yymmdd */

            m   = (pt.n[0]/100) % 100;
            day = pt.n[0] % 100;
            y   = pt.n[0]/10000;
            y   = convert_twodigit_year (y);
        }
    }

    /* See if we got anything valid out of all this. */
    /* y < 8000 is to catch 19998 style typos; the library is OK up to 65535 or so */
    if (y < 8000 && c_date_valid_dmy (day, m, y)) {
        d->month = m;
        d->day   = day;
        d->year  = y;
        d->dmy   = true;
    }
#ifdef G_ENABLE_DEBUG
    else {
      DEBUG_MSG (("Rejected DMY %u %u %u", day, m, y));
    }
#endif
    C_UNLOCK (gsDateLocker);
}

void c_date_set_time_t(CDate *date, time_t timet)
{
    struct tm tm;

    c_return_if_fail (date != NULL);

#ifdef HAVE_LOCALTIME_R
    localtime_r (&timet, &tm);
#else
    {
        struct tm *ptm = localtime (&timet);

        if (ptm == NULL) {
            /* Happens at least in Microsoft's C library if you pass a
             * negative time_t. Use 2000-01-01 as default date.
             */
            tm.tm_mon = 0;
            tm.tm_mday = 1;
            tm.tm_year = 100;
        }
        else {
            memcpy ((void *) &tm, (void *) ptm, sizeof(struct tm));
        }
    }
#endif

    date->julian = false;

    date->month = tm.tm_mon + 1;
    date->day   = tm.tm_mday;
    date->year  = tm.tm_year + 1900;

    c_return_if_fail (c_date_valid_dmy (date->day, date->month, date->year));

    date->dmy    = true;
}

void c_date_set_time(CDate *date, CTime time_)
{
    c_date_set_time_t (date, (time_t) time_);
}

void c_date_set_time_val(CDate *date, CTimeVal *timeval)
{
    c_date_set_time_t (date, (time_t) timeval->tvSec);
}

void c_date_set_month(CDate *d, CDateMonth m)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (c_date_valid_month (m));

    if (d->julian && !d->dmy) c_date_update_dmy(d);
    d->julian = false;

    d->month = m;

    if (c_date_valid_dmy (d->day, d->month, d->year))
        d->dmy = true;
    else
        d->dmy = false;
}

void c_date_set_day(CDate *d, CDateDay day)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (c_date_valid_day (day));

    if (d->julian && !d->dmy) c_date_update_dmy(d);
    d->julian = false;

    d->day = day;

    if (c_date_valid_dmy (d->day, d->month, d->year))
        d->dmy = true;
    else
        d->dmy = false;
}

void c_date_set_year(CDate *d, CDateYear y)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (c_date_valid_year (y));

    if (d->julian && !d->dmy) c_date_update_dmy(d);
    d->julian = false;

    d->year = y;

    if (c_date_valid_dmy (d->day, d->month, d->year))
        d->dmy = true;
    else
        d->dmy = false;
}

void c_date_set_dmy(CDate *d, CDateDay day, CDateMonth m, CDateYear y)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (c_date_valid_dmy (day, m, y));

    d->julian = false;

    d->month = m;
    d->day   = day;
    d->year  = y;

    d->dmy = true;
}

void c_date_set_julian(CDate *d, cuint32 j)
{
    c_return_if_fail (d != NULL);
    c_return_if_fail (c_date_valid_julian (j));

    d->julian_days = j;
    d->julian = true;
    d->dmy = false;
}

bool c_date_is_first_of_month(const CDate *d)
{
    c_return_val_if_fail (c_date_valid (d), false);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, false);

    if (d->day == 1) return true;
    else return false;

    return false;
}

bool c_date_is_last_of_month(const CDate *d)
{
    cint idx;

    c_return_val_if_fail (c_date_valid (d), false);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_val_if_fail (d->dmy, false);

    idx = c_date_is_leap_year (d->year) ? 1 : 0;

    if (d->day == gsDaysInMonths[idx][d->month]) return true;
    else return false;

    return false;
}

void c_date_add_days(CDate *d, cuint nDays)
{
    c_return_if_fail (c_date_valid (d));

    if (!d->julian)
        c_date_update_julian (d);

    c_return_if_fail (d->julian);
    c_return_if_fail (nDays <= C_MAX_UINT32 - d->julian_days);

    d->julian_days += nDays;
    d->dmy = false;
}

void c_date_subtract_days(CDate *d, cuint nDays)
{
    c_return_if_fail (c_date_valid (d));

    if (!d->julian)
        c_date_update_julian (d);

    c_return_if_fail (d->julian);
    c_return_if_fail (d->julian_days > nDays);

    d->julian_days -= nDays;
    d->dmy = false;
}

void c_date_add_months(CDate *d, cuint nMonths)
{
    cuint years, months;
    cint idx;

    c_return_if_fail (c_date_valid (d));

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_if_fail (d->dmy != 0);
    c_return_if_fail (nMonths <= C_MAX_UINT - (d->month - 1));

    nMonths += d->month - 1;

    years  = nMonths/12;
    months = nMonths%12;

    c_return_if_fail (years <= (cuint) (C_MAX_UINT16 - d->year));

    d->month = months + 1;
    d->year  += years;

    idx = c_date_is_leap_year (d->year) ? 1 : 0;

    if (d->day > gsDaysInMonths[idx][d->month]) {
        d->day = gsDaysInMonths[idx][d->month];
    }

    d->julian = false;

    c_return_if_fail (c_date_valid (d));
}

void c_date_subtract_months(CDate *d, cuint nMonths)
{
    cuint years, months;
    cint idx;

    c_return_if_fail (c_date_valid (d));

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_if_fail (d->dmy != 0);

    years  = nMonths/12;
    months = nMonths%12;

    c_return_if_fail (d->year > years);

    d->year  -= years;

    if (d->month > months) d->month -= months;
    else {
        months -= d->month;
        d->month = 12 - months;
        d->year -= 1;
    }

    idx = c_date_is_leap_year (d->year) ? 1 : 0;

    if (d->day > gsDaysInMonths[idx][d->month])
        d->day = gsDaysInMonths[idx][d->month];

    d->julian = false;

    c_return_if_fail (c_date_valid (d));
}

void c_date_add_years(CDate *d, cuint nYears)
{
    c_return_if_fail (c_date_valid (d));

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_if_fail (d->dmy != 0);
    c_return_if_fail (nYears <= (cuint) (C_MAX_UINT16 - d->year));

    d->year += nYears;

    if (d->month == 2 && d->day == 29) {
        if (!c_date_is_leap_year (d->year)) {
            d->day = 28;
        }
    }

    d->julian = false;
}

void c_date_subtract_years(CDate *d, cuint nYears)
{
    c_return_if_fail (c_date_valid (d));

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_if_fail (d->dmy != 0);
    c_return_if_fail (d->year > nYears);

    d->year -= nYears;

    if (d->month == 2 && d->day == 29) {
        if (!c_date_is_leap_year (d->year)) {
            d->day = 28;
        }
    }

    d->julian = false;
}

bool c_date_is_leap_year(CDateYear year)
{
    c_return_val_if_fail (c_date_valid_year (year), false);

    return ((((year % 4) == 0) && ((year % 100) != 0)) || (year % 400) == 0);
}

cuint8 c_date_get_days_in_month(CDateMonth month, CDateYear year)
{
    cint idx;

    c_return_val_if_fail (c_date_valid_year (year), 0);
    c_return_val_if_fail (c_date_valid_month (month), 0);

    idx = c_date_is_leap_year (year) ? 1 : 0;

    return gsDaysInMonths[idx][month];
}

cuint8 c_date_get_monday_weeks_in_year(CDateYear year)
{
    CDate d;

    c_return_val_if_fail (c_date_valid_year (year), 0);

    c_date_clear (&d, 1);
    c_date_set_dmy (&d, 1, 1, year);
    if (c_date_get_weekday (&d) == C_DATE_MONDAY) return 53;
    c_date_set_dmy (&d, 31, 12, year);
    if (c_date_get_weekday (&d) == C_DATE_MONDAY) return 53;
    if (c_date_is_leap_year (year)) {
        c_date_set_dmy (&d, 2, 1, year);
        if (c_date_get_weekday (&d) == C_DATE_MONDAY) return 53;
        c_date_set_dmy (&d, 30, 12, year);
        if (c_date_get_weekday (&d) == C_DATE_MONDAY) return 53;
    }
    return 52;
}

cuint8 c_date_get_sunday_weeks_in_year(CDateYear year)
{
    CDate d;

    c_return_val_if_fail (c_date_valid_year (year), 0);

    c_date_clear (&d, 1);
    c_date_set_dmy (&d, 1, 1, year);
    if (c_date_get_weekday (&d) == C_DATE_SUNDAY) return 53;
    c_date_set_dmy (&d, 31, 12, year);
    if (c_date_get_weekday (&d) == C_DATE_SUNDAY) return 53;
    if (c_date_is_leap_year (year)) {
        c_date_set_dmy (&d, 2, 1, year);
        if (c_date_get_weekday (&d) == C_DATE_SUNDAY) return 53;
        c_date_set_dmy (&d, 30, 12, year);
        if (c_date_get_weekday (&d) == C_DATE_SUNDAY) return 53;
    }
    return 52;
}

cint c_date_compare(const CDate *lhs, const CDate *rhs)
{
    c_return_val_if_fail (lhs != NULL, 0);
    c_return_val_if_fail (rhs != NULL, 0);
    c_return_val_if_fail (c_date_valid (lhs), 0);
    c_return_val_if_fail (c_date_valid (rhs), 0);

    /* Remember the self-comparison case! I think it works right now. */

    while (true) {
        if (lhs->julian && rhs->julian) {
            if (lhs->julian_days < rhs->julian_days) return -1;
            else if (lhs->julian_days > rhs->julian_days) return 1;
            else                                          return 0;
        }
        else if (lhs->dmy && rhs->dmy) {
            if (lhs->year < rhs->year)               return -1;
            else if (lhs->year > rhs->year)               return 1;
            else {
                if (lhs->month < rhs->month)         return -1;
                else if (lhs->month > rhs->month)         return 1;
                else {
                    if (lhs->day < rhs->day)              return -1;
                    else if (lhs->day > rhs->day)              return 1;
                    else                                       return 0;
                }

            }

        }
        else {
            if (!lhs->julian) c_date_update_julian (lhs);
            if (!rhs->julian) c_date_update_julian (rhs);
            c_return_val_if_fail (lhs->julian, 0);
            c_return_val_if_fail (rhs->julian, 0);
        }

    }
    return 0;
}

void c_date_to_struct_tm(const CDate *d, struct tm *tm)
{
    CDateWeekday day;

    c_return_if_fail (c_date_valid (d));
    c_return_if_fail (tm != NULL);

    if (!d->dmy)
        c_date_update_dmy (d);

    c_return_if_fail (d->dmy != 0);

    /* zero all the irrelevant fields to be sure they're valid */

    /* On Linux and maybe other systems, there are weird non-POSIX
     * fields on the end of struct tm that choke strftime if they
     * contain garbage.  So we need to 0 the entire struct, not just the
     * fields we know to exist.
     */

    memset (tm, 0x0, sizeof (struct tm));

    tm->tm_mday = d->day;
    tm->tm_mon  = d->month - 1; /* 0-11 goes in tm */
    tm->tm_year = ((int)d->year) - 1900; /* X/Open says tm_year can be negative */

    day = c_date_get_weekday (d);
    if (day == 7) day = 0; /* struct tm wants days since Sunday, so Sunday is 0 */

    tm->tm_wday = (int)day;

    tm->tm_yday = c_date_get_day_of_year (d) - 1; /* 0 to 365 */
    tm->tm_isdst = -1; /* -1 means "information not available" */
}

void c_date_clamp(CDate *date, const CDate *minDate, const CDate *maxDate)
{
    c_return_if_fail (c_date_valid (date));

    if (minDate != NULL) {
        c_return_if_fail (c_date_valid (minDate));
    }

    if (maxDate != NULL) {
        c_return_if_fail (c_date_valid (maxDate));
    }

    if (minDate != NULL && maxDate != NULL) {
        c_return_if_fail (c_date_compare (minDate, maxDate) <= 0);
    }

    if (minDate && c_date_compare (date, minDate) < 0) {
        *date = *minDate;
    }

    if (maxDate && c_date_compare (maxDate, date) < 0) {
        *date = *maxDate;
    }
}

void c_date_order(CDate *date1, CDate *date2)
{
    c_return_if_fail (c_date_valid (date1));
    c_return_if_fail (c_date_valid (date2));

    if (c_date_compare (date1, date2) > 0) {
        CDate tmp = *date1;
        *date1 = *date2;
        *date2 = tmp;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
csize c_date_strftime(char *s, csize slen, const char *format, const CDate *d)
{
    struct tm tm;
    csize locale_format_len = 0;
    char *locale_format;
    csize tmplen;
    char *tmpbuf;
    csize tmpbufsize;
    csize convlen = 0;
    char *convbuf;
    CError *error = NULL;
    csize retval;

    c_return_val_if_fail (c_date_valid (d), 0);
    c_return_val_if_fail (slen > 0, 0);
    c_return_val_if_fail (format != NULL, 0);
    c_return_val_if_fail (s != NULL, 0);

    c_date_to_struct_tm (d, &tm);

    locale_format = c_locale_from_utf8 (format, -1, NULL, &locale_format_len, &error);

    if (error) {
        C_LOG_WARNING_CONSOLE(C_STRLOC "Error converting format to locale encoding: %s", error->message);
        c_error_free (error);

        s[0] = '\0';
        return 0;
    }

    tmpbufsize = C_MAX (128, locale_format_len * 2);
    while (true) {
        tmpbuf = c_malloc0 (tmpbufsize);

        /* Set the first byte to something other than '\0', to be able to
         * recognize whether strftime actually failed or just returned "".
         */
        tmpbuf[0] = '\1';
        tmplen = strftime (tmpbuf, tmpbufsize, locale_format, &tm);

        if (tmplen == 0 && tmpbuf[0] != '\0') {
            c_free (tmpbuf);
            tmpbufsize *= 2;

            if (tmpbufsize > 65536) {
                C_LOG_WARNING_CONSOLE(C_STRLOC "Maximum buffer size for c_date_strftime exceeded: giving up");
                c_free (locale_format);

                s[0] = '\0';
                return 0;
            }
        }
        else
            break;
    }
    c_free (locale_format);

    convbuf = c_locale_to_utf8 (tmpbuf, tmplen, NULL, &convlen, &error);
    c_free (tmpbuf);

    if (error) {
        C_LOG_WARNING_CONSOLE (C_STRLOC "Error converting results of strftime to UTF-8: %s", error->message);
        c_error_free (error);

        c_assert (convbuf == NULL);

        s[0] = '\0';
        return 0;
    }

    if (slen <= convlen) {
        /* Ensure only whole characters are copied into the buffer.
         */
        char *end = c_utf8_find_prev_char (convbuf, convbuf + slen);
        c_assert (end != NULL);
        convlen = end - convbuf;

        /* Return 0 because the buffer isn't large enough.
         */
        retval = 0;
    }
    else {
        retval = convlen;
    }

    memcpy (s, convbuf, convlen);
    s[convlen] = '\0';
    c_free (convbuf);

    return retval;
}
#pragma GCC diagnostic pop



static void c_date_update_julian (const CDate *const_d)
{
    CDate *d = (CDate *) const_d;
    CDateYear year;
    cint idx;

    c_return_if_fail (d != NULL);
    c_return_if_fail (d->dmy != 0);
    c_return_if_fail (!d->julian);
    c_return_if_fail (c_date_valid_dmy (d->day, d->month, d->year));

    /* What we actually do is: multiply years * 365 days in the year,
     * add the number of years divided by 4, subtract the number of
     * years divided by 100 and add the number of years divided by 400,
     * which accounts for leap year stuff. Code from Steffen Beyer's
     * DateCalc.
     */

    year = d->year - 1; /* we know d->year > 0 since it's valid */

    d->julian_days = year * 365U;
    d->julian_days += (year >>= 2); /* divide by 4 and add */
    d->julian_days -= (year /= 25); /* divides original # years by 100 */
    d->julian_days += year >> 2;    /* divides by 4, which divides original by 400 */

    idx = c_date_is_leap_year (d->year) ? 1 : 0;

    d->julian_days += gsDaysInYear[idx][d->month] + d->day;

    c_return_if_fail (c_date_valid_julian (d->julian_days));

    d->julian = true;
}

static void c_date_update_dmy (const CDate *const_d)
{
    CDate *d = (CDate *) const_d;
    CDateYear y;
    CDateMonth m;
    CDateDay day;

    cuint32 A, B, C, D, E, M;

    c_return_if_fail (d != NULL);
    c_return_if_fail (d->julian);
    c_return_if_fail (!d->dmy);
    c_return_if_fail (c_date_valid_julian (d->julian_days));

    /* Formula taken from the Calendar FAQ; the formula was for the
     *  Julian Period which starts on 1 January 4713 BC, so we add
     *  1,721,425 to the number of days before doing the formula.
     *
     * I'm sure this can be simplified for our 1 January 1 AD period
     * start, but I can't figure out how to unpack the formula.
     */

    A = d->julian_days + 1721425 + 32045;
    B = ( 4 *(A + 36524) )/ 146097 - 1;
    C = A - (146097 * B)/4;
    D = ( 4 * (C + 365) ) / 1461 - 1;
    E = C - ((1461*D) / 4);
    M = (5 * (E - 1) + 2)/153;

    m = M + 3 - (12*(M/10));
    day = E - (153*M + 2)/5;
    y = 100 * B + D - 4800 + (M/10);

#ifdef G_ENABLE_DEBUG
    if (!c_date_valid_dmy (day, m, y))
    C_LOG_WARNING_CONSOLE ("OOPS julian: %u  computed dmy: %u %u %u",
               d->julian_days, day, m, y);
#endif

    d->month = m;
    d->day   = day;
    d->year  = y;

    d->dmy = true;
}

static inline bool update_month_match (csize *longest, const char *haystack, const char *needle)
{
    csize length;

    if (needle == NULL)
        return false;

    length = strlen (needle);
    if (*longest >= length)
        return false;

    if (strstr (haystack, needle) == NULL)
        return false;

    *longest = length;
    return true;
}

/* HOLDS: c_date_global_lock */
static void c_date_prepare_to_parse (const char* str, CDateParseTokens *pt)
{
    const char *locale = setlocale (LC_TIME, NULL);
    bool recompute_localeinfo = false;
    CDate d;

    c_return_if_fail (locale != NULL); /* should not happen */

    c_date_clear (&d, 1);              /* clear for scratch use */

    if ( (gsCurrentLocale == NULL) || (strcmp (locale, gsCurrentLocale) != 0) )
        recompute_localeinfo = true;  /* Uh, there used to be a reason for the temporary */

    if (recompute_localeinfo)
    {
        int i = 1;
        CDateParseTokens testpt;
        char buf[128];

        c_free (gsCurrentLocale); /* still works if current_locale == NULL */
        gsCurrentLocale = c_strdup (locale);

        gsShortMonthNames[0] = "Error";
        gsLongMonthNames[0] = "Error";

        while (i < 13) {
            char *casefold;

            c_date_set_dmy (&d, 1, i, 1976);

            c_return_if_fail (c_date_valid (&d));

            c_date_strftime (buf, 127, "%b", &d);

            casefold = c_utf8_casefold (buf, -1);
            c_free (gsShortMonthNames[i]);
            gsShortMonthNames[i] = c_utf8_normalize (casefold, -1, C_NORMALIZE_ALL);
            c_free (casefold);

            c_date_strftime (buf, 127, "%B", &d);
            casefold = c_utf8_casefold (buf, -1);
            c_free (gsLongMonthNames[i]);
            gsLongMonthNames[i] = c_utf8_normalize (casefold, -1, C_NORMALIZE_ALL);
            c_free (casefold);

            c_date_strftime (buf, 127, "%Ob", &d);
            casefold = c_utf8_casefold (buf, -1);
            c_free (gsShortMonthNamesAlternative[i]);
            gsShortMonthNamesAlternative[i] = c_utf8_normalize (casefold, -1, C_NORMALIZE_ALL);
            c_free (casefold);

            c_date_strftime (buf, 127, "%OB", &d);
            casefold = c_utf8_casefold (buf, -1);
            c_free (gsLongMonthNamesAlternative[i]);
            gsLongMonthNamesAlternative[i] = c_utf8_normalize (casefold, -1, C_NORMALIZE_ALL);
            c_free (casefold);
            ++i;
        }

        /* Determine DMY order */

        /* had to pick a random day - don't change this, some strftimes
         * are broken on some days, and this one is good so far. */
        c_date_set_dmy (&d, 4, 7, 1976);

        c_date_strftime (buf, 127, "%x", &d);

        c_date_fill_parse_tokens (buf, &testpt);

        gsUsingTwoDigitYears = false;
        gsLocaleEraAdjust = 0;
        gsDmyOrder[0] = C_DATE_DAY;
        gsDmyOrder[1] = C_DATE_MONTH;
        gsDmyOrder[2] = C_DATE_YEAR;

        i = 0;
        while (i < testpt.numInts) {
            switch (testpt.n[i]) {
                case 7:
                    gsDmyOrder[i] = C_DATE_MONTH;
                    break;
                case 4:
                    gsDmyOrder[i] = C_DATE_DAY;
                    break;
                case 76:
                    gsUsingTwoDigitYears = true;
                    C_FALLTHROUGH;
                case 1976:
                    gsDmyOrder[i] = C_DATE_YEAR;
                    break;
                default:
                    /* assume locale era */
                    gsLocaleEraAdjust = 1976 - testpt.n[i];
                    gsDmyOrder[i] = C_DATE_YEAR;
                    break;
            }
            ++i;
        }

#if defined(G_ENABLE_DEBUG) && 0
        DEBUG_MSG (("**CDate prepared a new set of locale-specific parse rules."));
      i = 1;
      while (i < 13)
        {
          DEBUG_MSG (("  %s   %s", long_month_names[i], short_month_names[i]));
          ++i;
        }
      DEBUG_MSG (("Alternative month names:"));
      i = 1;
      while (i < 13)
        {
          DEBUG_MSG (("  %s   %s", long_month_names_alternative[i], short_month_names_alternative[i]));
          ++i;
        }
      if (using_twodigit_years)
        {
          DEBUG_MSG (("**Using twodigit years with cutoff year: %u", twodigit_start_year));
        }
      {
        char *strings[3];
        i = 0;
        while (i < 3)
          {
            switch (dmy_order[i])
              {
              case c_date_MONTH:
                strings[i] = "Month";
                break;
              case c_date_YEAR:
                strings[i] = "Year";
                break;
              case c_date_DAY:
                strings[i] = "Day";
                break;
              default:
                strings[i] = NULL;
                break;
              }
            ++i;
          }
        DEBUG_MSG (("**Order: %s, %s, %s", strings[0], strings[1], strings[2]));
        DEBUG_MSG (("**Sample date in this locale: '%s'", buf));
      }
#endif
    }

    c_date_fill_parse_tokens (str, pt);
}

static cuint convert_twodigit_year (cuint y)
{
    if (gsUsingTwoDigitYears && y < 100) {
        cuint two     =  gsTwoDigitStartYear % 100;
        cuint century = (gsTwoDigitStartYear / 100) * 100;
        if (y < two) {
            century += 100;
        }

        y += century;
    }
    return y;
}

static void c_date_fill_parse_tokens (const char *str, CDateParseTokens *pt)
{
    char num[4][NUM_LEN+1];
    int i;
    const cuchar *s;

    /* We count 4, but store 3; so we can give an error
     * if there are 4.
     */
    num[0][0] = num[1][0] = num[2][0] = num[3][0] = '\0';

    s = (const cuchar *) str;
    pt->numInts = 0;
    while (*s && pt->numInts < 4) {

        i = 0;
        while (*s && c_ascii_isdigit (*s) && i < NUM_LEN) {
            num[pt->numInts][i] = *s;
            ++s;
            ++i;
        }

        if (i > 0) {
            num[pt->numInts][i] = '\0';
            ++(pt->numInts);
        }

        if (*s == '\0') break;

        ++s;
    }

    pt->n[0] = pt->numInts > 0 ? atoi (num[0]) : 0;
    pt->n[1] = pt->numInts > 1 ? atoi (num[1]) : 0;
    pt->n[2] = pt->numInts > 2 ? atoi (num[2]) : 0;

    pt->month = C_DATE_BAD_MONTH;

    if (pt->numInts < 3) {
        csize longest = 0;
        char *casefold;
        char *normalized;

        casefold = c_utf8_casefold (str, -1);
        normalized = c_utf8_normalize (casefold, -1, C_NORMALIZE_ALL);
        c_free (casefold);

        for (i = 1; i < 13; ++i) {
            /* Here month names may be in a genitive case if the language
             * grammatical rules require it.
             * Examples of how January may look in some languages:
             * Catalan: "de gener", Croatian: "siječnja", Polish: "stycznia",
             * Upper Sorbian: "januara".
             * Note that most of the languages can't or don't use the the
             * genitive case here so they use nominative everywhere.
             * For example, English always uses "January".
             */
            if (update_month_match (&longest, normalized, gsLongMonthNames[i]))
                pt->month = i;

            /* Here month names will be in a nominative case.
             * Examples of how January may look in some languages:
             * Catalan: "gener", Croatian: "Siječanj", Polish: "styczeń",
             * Upper Sorbian: "Januar".
             */
            if (update_month_match (&longest, normalized, gsLongMonthNamesAlternative[i]))
                pt->month = i;

            /* Differences between abbreviated nominative and abbreviated
             * genitive month names are visible in very few languages but
             * let's handle them.
             */
            if (update_month_match (&longest, normalized, gsShortMonthNames[i]))
                pt->month = i;

            if (update_month_match (&longest, normalized, gsShortMonthNamesAlternative[i]))
                pt->month = i;
        }

        c_free (normalized);
    }
}


