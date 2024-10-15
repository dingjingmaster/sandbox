
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

#ifndef CLIBRARY_DATE_H
#define CLIBRARY_DATE_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <time.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

#define C_DATE_BAD_JULIAN 0U
#define C_DATE_BAD_DAY    0U
#define C_DATE_BAD_YEAR   0U


typedef cint32  CTime;
typedef cuint16 CDateYear;
typedef cuint8  CDateDay;           // day of the month
typedef struct _CDate CDate;

/* enum used to specify order of appearance in parsed date strings */
typedef enum
{
    C_DATE_DAY   = 0,
    C_DATE_MONTH = 1,
    C_DATE_YEAR  = 2
} CDateDMY;

/* actual week and month values */
typedef enum
{
    C_DATE_BAD_WEEKDAY  = 0,
    C_DATE_MONDAY       = 1,
    C_DATE_TUESDAY      = 2,
    C_DATE_WEDNESDAY    = 3,
    C_DATE_THURSDAY     = 4,
    C_DATE_FRIDAY       = 5,
    C_DATE_SATURDAY     = 6,
    C_DATE_SUNDAY       = 7
} CDateWeekday;

typedef enum
{
    C_DATE_BAD_MONTH = 0,
    C_DATE_JANUARY   = 1,
    C_DATE_FEBRUARY  = 2,
    C_DATE_MARCH     = 3,
    C_DATE_APRIL     = 4,
    C_DATE_MAY       = 5,
    C_DATE_JUNE      = 6,
    C_DATE_JULY      = 7,
    C_DATE_AUGUST    = 8,
    C_DATE_SEPTEMBER = 9,
    C_DATE_OCTOBER   = 10,
    C_DATE_NOVEMBER  = 11,
    C_DATE_DECEMBER  = 12
} CDateMonth;

struct _CDate
{
    cuint julian_days : 32;
    cuint julian : 1;       // julian is valid
    cuint dmy    : 1;       // dmy is valid

    /* DMY representation */
    cuint day    : 6;
    cuint month  : 4;
    cuint year   : 16;
};

CDate*          c_date_new                      (void);
CDate*          c_date_new_dmy                  (CDateDay day, CDateMonth month, CDateYear year);
CDate*          c_date_new_julian               (cuint32 julianDay);
void            c_date_free                     (CDate* date);
CDate*          c_date_copy                     (const CDate* date);

bool            c_date_valid                    (const CDate* date);
bool            c_date_valid_day                (CDateDay day) C_CONST;
bool            c_date_valid_month              (CDateMonth month) C_CONST;
bool            c_date_valid_year               (CDateYear year) C_CONST;
bool            c_date_valid_weekday            (CDateWeekday weekday) C_CONST;
bool            c_date_valid_julian             (cuint32 julianDate) C_CONST;
bool            c_date_valid_dmy                (CDateDay day, CDateMonth month, CDateYear year) C_CONST;

CDateWeekday    c_date_get_weekday              (const CDate* date);
CDateMonth      c_date_get_month                (const CDate* date);
CDateYear       c_date_get_year                 (const CDate* date);
CDateDay        c_date_get_day                  (const CDate* date);
cuint32         c_date_get_julian               (const CDate* date);
cuint           c_date_get_day_of_year          (const CDate* date);
cuint           c_date_get_monday_week_of_year  (const CDate* date);
cuint           c_date_get_sunday_week_of_year  (const CDate* date);
cuint           c_date_get_iso8601_week_of_year (const CDate* date);

void            c_date_clear                    (CDate* date, cuint nDates);
void            c_date_set_parse                (CDate* date, const char* str);
void            c_date_set_time_t               (CDate* date, time_t timet);
void            c_date_set_time_val             (CDate* date, CTimeVal* timeval);
void            c_date_set_time                 (CDate* date, CTime time_);
void            c_date_set_month                (CDate* date, CDateMonth month);
void            c_date_set_day                  (CDate* date, CDateDay day);
void            c_date_set_year                 (CDate* date, CDateYear year);
void            c_date_set_dmy                  (CDate* date, CDateDay day, CDateMonth month, CDateYear y);
void            c_date_set_julian               (CDate* date, cuint32 julianDate);
bool            c_date_is_first_of_month        (const CDate* date);
bool            c_date_is_last_of_month         (const CDate* date);
void            c_date_add_days                 (CDate* date, cuint nDays);
void            c_date_subtract_days            (CDate* date, cuint nDays);
void            c_date_add_months               (CDate* date, cuint nMonths);
void            c_date_subtract_months          (CDate* date, cuint nMonths);
void            c_date_add_years                (CDate* date, cuint nYears);
void            c_date_subtract_years           (CDate* date, cuint nYears);
bool            c_date_is_leap_year             (CDateYear year) C_CONST;
cuint8          c_date_get_days_in_month        (CDateMonth month, CDateYear year) C_CONST;
cuint8          c_date_get_monday_weeks_in_year (CDateYear year) C_CONST;
cuint8          c_date_get_sunday_weeks_in_year (CDateYear year) C_CONST;
cint            c_date_days_between             (const CDate* date1, const CDate* date2);
cint            c_date_compare                  (const CDate* lhs, const CDate* rhs);
void            c_date_to_struct_tm             (const CDate* date, struct tm* tm);
void            c_date_clamp                    (CDate* date, const CDate* minDate, const CDate* maxDate);
void            c_date_order                    (CDate* date1, CDate* date2);
csize           c_date_strftime                 (char* s, csize slen, const char* format, const CDate* date);


C_END_EXTERN_C

#endif //CLIBRARY_DATE_H
