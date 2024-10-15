
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

#include <time.h>
#include "timer.h"
#include "clib.h"


static time_t mktime_utc (struct tm *tm);


CTimer* c_timer_new (void)
{
    CTimer* timer = c_malloc0(sizeof(CTimer));

    timer->active = true;
    timer->start = c_get_monotonic_time ();

    return timer;
}

void c_timer_destroy (CTimer* timer)
{
    c_return_if_fail (timer != NULL);

    c_free (timer);
}

void c_timer_start (CTimer* timer)
{
    c_return_if_fail (timer != NULL);

    timer->active = true;

    timer->start = c_get_monotonic_time ();
}

void c_timer_stop (CTimer* timer)
{
    c_return_if_fail (timer != NULL);

    timer->active = false;

    timer->end = c_get_monotonic_time ();
}

void c_timer_reset (CTimer* timer)
{
    c_return_if_fail (timer != NULL);

    timer->start = c_get_monotonic_time ();
}

void c_timer_continue (CTimer* timer)
{
    c_return_if_fail (timer != NULL);
    c_return_if_fail (timer->active == false);

    cuint64 elapsed = timer->end - timer->start;

    timer->start = c_get_monotonic_time ();

    timer->start -= elapsed;

    timer->active = true;
}

double c_timer_elapsed (CTimer* timer, culong* microseconds)
{
    cdouble total;

    c_return_val_if_fail (timer != NULL, 0);

    if (timer->active) {
        timer->end = c_get_monotonic_time ();
    }

    cint64 elapsed = (cint64) (timer->end - timer->start);

    total = (cdouble) ((cdouble) elapsed / 1e6);

    if (microseconds) {
        *microseconds = elapsed % 1000000;
    }

    return total;
}

bool c_timer_is_active (CTimer* timer)
{
    c_return_val_if_fail (timer != NULL, false);

    return (bool) timer->active;
}

void c_usleep (culong microseconds)
{
    struct timespec request, remaining;
    request.tv_sec = (long) (microseconds / C_USEC_PER_SEC);
    request.tv_nsec = (long) (1000 * (microseconds % C_USEC_PER_SEC));
    while (nanosleep (&request, &remaining) == -1 && errno == EINTR) {
        request = remaining;
    }
}

void c_time_val_add (CTimeVal* time_, clong microseconds)
{
    c_return_if_fail (time_ != NULL && time_->tvUsec >= 0 && time_->tvUsec < C_USEC_PER_SEC);

    if (microseconds >= 0) {
        time_->tvUsec += microseconds % C_USEC_PER_SEC;
        time_->tvSec += microseconds / C_USEC_PER_SEC;
        if (time_->tvUsec >= C_USEC_PER_SEC) {
            time_->tvUsec -= C_USEC_PER_SEC;
            time_->tvSec++;
        }
    }
    else {
        microseconds *= -1;
        time_->tvUsec -= microseconds % C_USEC_PER_SEC;
        time_->tvSec -= microseconds / C_USEC_PER_SEC;
        if (time_->tvUsec < 0) {
            time_->tvUsec += C_USEC_PER_SEC;
            time_->tvSec--;
        }
    }
}

bool c_time_val_from_iso8601 (const char* isoDate, CTimeVal* time_)
{
    struct tm tm = {0};
    long val;
    long mday, mon, year;
    long hour, min, sec;

    c_return_val_if_fail (isoDate != NULL, false);
    c_return_val_if_fail (time_ != NULL, false);

    while (c_ascii_isspace (*isoDate)) {
        isoDate++;
    }

    if (*isoDate == '\0') {
        return false;
    }

    if (!c_ascii_isdigit (*isoDate) && *isoDate != '+') {
        return false;
    }

    val = (long) strtoul (isoDate, (char**) &isoDate, 10);
    if (*isoDate == '-') {
        /* YYYY-MM-DD */
        year = val;
        isoDate++;

        mon = (long) strtoul (isoDate, (char **) &isoDate, 10);
        if (*isoDate++ != '-') {
            return false;
        }

        mday = (long) strtoul (isoDate, (char **) &isoDate, 10);
    }
    else {
        /* YYYYMMDD */
        mday = val % 100;
        mon = (val % 10000) / 100;
        year = val / 10000;
    }

    /* Validation. */
    if (year < 1900 || year > C_MAX_INT32) {
        return false;
    }
    if (mon < 1 || mon > 12) {
        return false;
    }
    if (mday < 1 || mday > 31) {
        return false;
    }

    tm.tm_mday = (int) mday;
    tm.tm_mon = (int) mon - 1;
    tm.tm_year = (int) year - 1900;

    if (*isoDate != 'T') {
        return false;
    }

    isoDate++;

    /* If there is a 'T' then there has to be a time */
    if (!c_ascii_isdigit (*isoDate)) {
        return false;
    }

    val = (long) strtoul (isoDate, (char**) &isoDate, 10);
    if (*isoDate == ':') {
        /* hh:mm:ss */
        hour = val;
        isoDate++;
        min = (long) strtoul (isoDate, (char**) &isoDate, 10);
        if (*isoDate++ != ':') {
            return false;
        }
        sec = (long) strtoul (isoDate, (char**) &isoDate, 10);
    }
    else {
        /* hhmmss */
        sec = val % 100;
        min = (val % 10000) / 100;
        hour = val / 10000;
    }

    /* Validation. Allow up to 2 leap seconds when validating @sec. */
    if (hour > 23) {
        return false;
    }

    if (min > 59) {
        return false;
    }

    if (sec > 61) {
        return false;
    }

    tm.tm_hour = (int) hour;
    tm.tm_min = (int) min;
    tm.tm_sec = (int) sec;

    time_->tvUsec = 0;

    if (*isoDate == ',' || *isoDate == '.') {
        clong mul = 100000;

        while (mul >= 1 && c_ascii_isdigit (*++isoDate)) {
            time_->tvUsec += (*isoDate - '0') * mul;
            mul /= 10;
        }

        /* Skip any remaining digits after we’ve reached our limit of precision. */
        while (c_ascii_isdigit (*isoDate)) {
            isoDate++;
        }
    }

    /* Now parse the offset and convert tm to a time_t */
    if (*isoDate == 'Z') {
        isoDate++;
        time_->tvSec = mktime_utc (&tm);
    }
    else if (*isoDate == '+' || *isoDate == '-') {
        cint sign = (*isoDate == '+') ? -1 : 1;
        val = (long) strtoul (isoDate + 1, (char**) &isoDate, 10);
        if (*isoDate == ':') {
            /* hh:mm */
            hour = val;
            min = (long) strtoul (isoDate + 1, (char**) &isoDate, 10);
        }
        else {
            /* hhmm */
            hour = val / 100;
            min = val % 100;
        }

        if (hour > 99) {
            return false;
        }

        if (min > 59) {
            return false;
        }

        time_->tvSec = mktime_utc (&tm) + (time_t) (60 * (cint64) (60 * hour + min) * sign);
    }
    else {
        /* No "Z" or offset, so local time */
        tm.tm_isdst = -1; /* locale selects DST */
        time_->tvSec = mktime (&tm);
    }

    while (c_ascii_isspace (*isoDate)) {
        isoDate++;
    }

    return *isoDate == '\0';
}

char* c_time_val_to_iso8601 (CTimeVal* time_)
{
    char* retVal;
    struct tm *tm;
    time_t secs;

    c_return_val_if_fail (time_ != NULL && time_->tvUsec >= 0 && time_->tvUsec < C_USEC_PER_SEC, NULL);

    secs = time_->tvSec;
    tm = gmtime (&secs);

    /* If the gmtime() call has failed, time_->tv_sec is too big. */
    if (tm == NULL) {
        return NULL;
    }

    if (time_->tvUsec != 0) {
        /* ISO 8601 date and time format, with fractionary seconds:
         *   YYYY-MM-DDTHH:MM:SS.MMMMMMZ
         */
        retVal = c_strdup_printf ("%4d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
                                    tm->tm_year + 1900,
                                    tm->tm_mon + 1,
                                    tm->tm_mday,
                                    tm->tm_hour,
                                    tm->tm_min,
                                    tm->tm_sec,
                                    time_->tvUsec);
    }
    else {
        /* ISO 8601 date and time format:
         *   YYYY-MM-DDTHH:MM:SSZ
         */
        retVal = c_strdup_printf ("%4d-%02d-%02dT%02d:%02d:%02dZ",
                                    tm->tm_year + 1900,
                                    tm->tm_mon + 1,
                                    tm->tm_mday,
                                    tm->tm_hour,
                                    tm->tm_min,
                                    tm->tm_sec);
    }

    return retVal;
}

static time_t mktime_utc (struct tm *tm)
{
    time_t retVal;

    static const cint daysBefore[] =
        {
            0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
        };

    if (tm->tm_mon < 0 || tm->tm_mon > 11) {
        return (time_t) -1;
    }

    retVal = (tm->tm_year - 70) * 365;
    retVal += (tm->tm_year - 68) / 4;
    retVal += daysBefore[tm->tm_mon] + tm->tm_mday - 1;

    if (tm->tm_year % 4 == 0 && tm->tm_mon < 2) {
        retVal -= 1;
    }

    retVal = ((((retVal * 24) + tm->tm_hour) * 60) + tm->tm_min) * 60 + tm->tm_sec;

    return retVal;
}
