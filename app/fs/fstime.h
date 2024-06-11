//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_FSTIME_H
#define sandbox_FSTIME_H
#include <c/clib.h>

#include "types.h"
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

C_BEGIN_EXTERN_C

#define FS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

/*
 * assume "struct timespec" is not defined if st_mtime is not defined
 */
#if !defined(st_mtime) & !defined(__timespec_defined)
struct timespec
{
        time_t tv_sec;
        long tv_nsec;
} ;
#endif


typedef sle64 FSTime;


static __inline__ struct timespec ntfs2timespec(FSTime ntfstime)
{
        struct timespec spec;
        s64 cputime;

        cputime = sle64_to_cpu(ntfstime);
        spec.tv_sec = (cputime - (FS_TIME_OFFSET)) / 10000000;
        spec.tv_nsec = (cputime - (FS_TIME_OFFSET)
                        - (s64)spec.tv_sec*10000000)*100;
                /* force zero nsec for overflowing dates */
        if ((spec.tv_nsec < 0) || (spec.tv_nsec > 999999999))
                spec.tv_nsec = 0;
        return (spec);
}


static __inline__ FSTime timespec2ntfs(struct timespec spec)
{
        s64 units;

        units = (s64)spec.tv_sec * 10000000 + FS_TIME_OFFSET + spec.tv_nsec/100;
        return (cpu_to_sle64(units));
}

static __inline__ FSTime ntfs_current_time(void)
{
        struct timespec now;

#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_SYS_CLOCK_GETTIME)
        clock_gettime(CLOCK_REALTIME, &now);
#elif defined(HAVE_GETTIMEOFDAY)
        struct timeval microseconds;

        gettimeofday(&microseconds, (struct timezone*)NULL);
        now.tv_sec = microseconds.tv_sec;
        now.tv_nsec = microseconds.tv_usec*1000;
#else
        now.tv_sec = time((time_t*)NULL);
        now.tv_nsec = 0;
#endif
        return (timespec2ntfs(now));
}

C_END_EXTERN_C

#endif // sandbox_FSTIME_H
