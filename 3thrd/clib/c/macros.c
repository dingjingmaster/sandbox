
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-9.
//

#include <time.h>
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "log.h"
#include "str.h"
#include "timer.h"
#include "thread.h"
#include "macros.h"
#include "source.h"
#include "file-utils.h"

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "clibrary"
#endif

#ifndef CLIB_LOCALE_DIR
#define CLIB_LOCALE_DIR "/usr/share/locale"
#endif

#ifndef CLIB_LOCALE
#define CLIB_LOCALE "zh_CN.UTF-8"
#endif


typedef struct _MSortParam          MSortParam;

struct _MSortParam
{
    cuint64                 s;
    cuint64                 var;
    CCompareDataFunc        cmp;
    void*                   arg;
    char*                   t;
};


/* rand start */
C_LOCK_DEFINE_STATIC (gsGlobalRandom);
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

#define C_RAND_DOUBLE_TRANSFORM 2.3283064365386962890625e-10


struct _CRand
{
    cuint32         mt[N]; /* the array for the state vector  */
    cuint           mti;
};
/* rand end */

static cuint get_random_version (void);
static CRand* get_global_random (void);
static void msort_with_tmp (const MSortParam* p, void* b, size_t n);
static void msort_r (void *b, cuint64 n, cuint64 s, CCompareDataFunc cmp, void *arg);

static void ensure_gettext_initialized (void);
static bool _c_dgettext_should_translate (void);
static int c_environ_find (char** envp, const char* variable);
static bool c_environ_matches (const char* env, const char* variable, csize len);
static char* c_build_filename_va (const char* firstArgument, va_list* args, char** strArray);
static char** c_environ_unsetenv_internal (char** envp, const char* variable, bool freeValue);

extern char** environ;


void c_abort (void)
{
    C_LOG_WARNING_CONSOLE("c_abort!");

    abort();
}

void c_qsort_with_data (const void* pBase, cint totalElems, csize size, CCompareDataFunc compareFunc, void* udata)
{
    msort_r ((void*) pBase, totalElems, size, compareFunc, udata);
}

bool c_direct_equal (const void* p1, const void* p2)
{
    return p1 == p2;
}

bool c_str_equal (const void* p1, const void* p2)
{
    return 0 == strcmp (p1, p2);
}

bool c_int_equal (const void* p1, const void* p2)
{
    return *(cint*) p1 == *(cint*) p2;
}

bool c_int64_equal (const void* p1, const void* p2)
{
    return *(cint64*) p1 == *(cint64*) p2;
}

bool c_double_equal (const void* p1, const void* p2)
{
    return *(cdouble*) p1 == *(cdouble*) p2;
}


CRand* c_rand_new_with_seed (cuint32  seed)
{
    CRand *rand = c_malloc0(sizeof (CRand));
    c_rand_set_seed (rand, seed);

    return rand;
}

CRand* c_rand_new_with_seed_array (const cuint32 *seed, cuint seedLength)
{
    CRand *rand = c_malloc0(sizeof(CRand));
    c_rand_set_seed_array (rand, seed, seedLength);

    return rand;
}

CRand* c_rand_new (void)
{
    cuint32 seed[4];
    static bool devUrandomExists = true;

    if (devUrandomExists) {
        FILE* devUrandom;
        do {
            devUrandom = fopen("/dev/urandom", "rb");
        }
        while C_UNLIKELY (devUrandom == NULL && errno == EINTR);

        if (devUrandom) {
            int r;
            setvbuf (devUrandom, NULL, _IONBF, 0);
            do {
                errno = 0;
                r = fread (seed, (int) sizeof (seed), 1, devUrandom);
            }
            while C_UNLIKELY (errno == EINTR);

            if (r != 1) {
                devUrandomExists = false;
            }
            fclose (devUrandom);
        }
        else {
            devUrandomExists = false;
        }
    }

    if (!devUrandomExists) {
        cint64 nowUs = c_get_real_time ();
        seed[0] = nowUs / C_USEC_PER_SEC;
        seed[1] = nowUs % C_USEC_PER_SEC;
        seed[2] = getpid ();
        seed[3] = getppid ();
    }

    return c_rand_new_with_seed_array (seed, 4);
}

void c_rand_free (CRand* rand)
{
    c_return_if_fail (rand);

    c_free (rand);
}

CRand* c_rand_copy (CRand* rand)
{
    CRand* newRand;

    c_return_val_if_fail (rand != NULL, NULL);

    newRand = c_malloc0(sizeof(CRand));
    memcpy (newRand, rand, sizeof (CRand));

    return newRand;
}

void c_rand_set_seed (CRand* rand, cuint32 seed)
{
    c_return_if_fail (rand != NULL);

    switch (get_random_version ()) {
        case 20: {
            /* setting initial seeds to mt[N] using         */
            /* the generator Line 25 of Table 1 in          */
            /* [KNUTH 1981, The Art of Computer Programming */
            /*    Vol. 2 (2nd Ed.), pp102]                  */

            if (seed == 0) { /* This would make the PRNG produce only zeros */
                seed = 0x6b842128; /* Just set it to another number */
            }
            rand->mt[0]= seed;
            for (rand->mti=1; rand->mti<N; rand->mti++) {
                rand->mt[rand->mti] = (69069 * rand->mt[rand->mti - 1]);
            }
            break;
        }
        case 22: {
            /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
            /* In the previous version (see above), MSBs of the    */
            /* seed affect only MSBs of the array mt[].            */

            rand->mt[0]= seed;
            for (rand->mti=1; rand->mti<N; rand->mti++) {
                rand->mt[rand->mti] = 1812433253UL * (rand->mt[rand->mti - 1] ^ (rand->mt[rand->mti - 1] >> 30)) + rand->mti;
            }
            break;
        }
        default: {
            c_assert_not_reached();
        }
    }
}

void c_rand_set_seed_array (CRand* rand, const cuint32* seed, cuint seedLength)
{
    cuint i, j, k;

    c_return_if_fail (rand != NULL);
    c_return_if_fail (seedLength >= 1);

    c_rand_set_seed (rand, 19650218UL);

    i=1; j=0;
    k = (N > seedLength ? N : seedLength);
    for (; k; k--) {
        rand->mt[i] = (rand->mt[i] ^ ((rand->mt[i-1] ^ (rand->mt[i-1] >> 30)) * 1664525UL)) + seed[j] + j; /* non linear */
        rand->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++; j++;
        if (i >= N) {
            rand->mt[0] = rand->mt[N-1];
            i=1;
        }
        if (j >= seedLength) {
            j = 0;
        }
    }
    for (k = N - 1; k; k--) {
        rand->mt[i] = (rand->mt[i] ^ ((rand->mt[i-1] ^ (rand->mt[i-1] >> 30)) * 1566083941UL)) - i; /* non linear */
        rand->mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
        i++;
        if (i >= N) {
            rand->mt[0] = rand->mt[N-1];
            i=1;
        }
    }

    rand->mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
}

cuint32 c_rand_int (CRand* rand)
{
    cuint32 y;
    static const cuint32 mag01[2] = {0x0, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    c_return_val_if_fail (rand != NULL, 0);

    if (rand->mti >= N) { /* generate N words at one time */
        int kk;

        for (kk = 0; kk < N - M; kk++) {
            y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk + 1] & LOWER_MASK);
            rand->mt[kk] = rand->mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        for (; kk < N - 1; kk++) {
            y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk+1] & LOWER_MASK);
            rand->mt[kk] = rand->mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        y = (rand->mt[N - 1] & UPPER_MASK) | (rand->mt[0] & LOWER_MASK);
        rand->mt[N - 1] = rand->mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1];
        rand->mti = 0;
    }

    y = rand->mt[rand->mti++];
    y ^= TEMPERING_SHIFT_U(y);
    y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
    y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
    y ^= TEMPERING_SHIFT_L(y);

    return y;
}

cint32 c_rand_int_range (CRand* rand, cint32 begin, cint32 end)
{
    cuint32 dist = end - begin;
    cuint32 random = 0;

    c_return_val_if_fail (rand != NULL, begin);
    c_return_val_if_fail (end > begin, begin);

    switch (get_random_version ()) {
        case 20: {
            if (dist <= 0x10000L) {
                /* This method, which only calls g_rand_int once is only good
                 * for (end - begin) <= 2^16, because we only have 32 bits set
                 * from the one call to g_rand_int ().
                 *
                 * We are using (trans + trans * trans), because g_rand_int only
                 * covers [0..2^32-1] and thus g_rand_int * trans only covers
                 * [0..1-2^-32], but the biggest double < 1 is 1-2^-52.
                 */

                double doubleRand = c_rand_int (rand) * (C_RAND_DOUBLE_TRANSFORM + C_RAND_DOUBLE_TRANSFORM * C_RAND_DOUBLE_TRANSFORM);
                random = (cint32) (doubleRand * dist);
            }
            else {
                /* Now we use g_rand_double_range (), which will set 52 bits
                 * for us, so that it is safe to round and still get a decent
                 * distribution
                 */
                random = (cint32) c_rand_double_range (rand, 0, dist);
            }
            break;
        }
        case 22: {
            if (dist == 0) {
                random = 0;
            }
            else {
                /* maxvalue is set to the predecessor of the greatest
                 * multiple of dist less or equal 2^32.
                 */
                cuint32 maxvalue;
                if (dist <= 0x80000000u) {
                    /* maxvalue = 2^32 - 1 - (2^32 % dist) */
                    cuint32 leftover = (0x80000000u % dist) * 2;
                    if (leftover >= dist) {
                        leftover -= dist;
                    }
                    maxvalue = 0xffffffffu - leftover;
                } else {
                    maxvalue = dist - 1;
                }

                do {
                    random = c_rand_int (rand);
                }
                while (random > maxvalue);

                random %= dist;
            }
            break;
        }
        default: {
            c_assert_not_reached ();
        }
    }

    return begin + random;
}

cdouble c_rand_double (CRand* rand)
{
    cdouble retVal = c_rand_int (rand) * C_RAND_DOUBLE_TRANSFORM;
    retVal = (retVal + c_rand_int (rand)) * C_RAND_DOUBLE_TRANSFORM;

    if (retVal >= 1.0) {
        return c_rand_double (rand);
    }

    return retVal;
}

cdouble c_rand_double_range (CRand* rand, cdouble begin, cdouble end)
{
    double r = c_rand_double (rand);

    return r * end - (r - 1) * begin;
}

void c_random_set_seed (cuint32 seed)
{
    C_LOCK (gsGlobalRandom);
    c_rand_set_seed (get_global_random (), seed);
    C_UNLOCK (gsGlobalRandom);
}

cuint32 c_random_int (void)
{
    cuint32 result;
    C_LOCK (gsGlobalRandom);
    result = c_rand_int (get_global_random ());
    C_UNLOCK (gsGlobalRandom);

    return result;
}

cint32 c_random_int_range (cint32 begin, cint32 end)
{
    cint32 result;
    C_LOCK (gsGlobalRandom);
    result = c_rand_int_range (get_global_random (), begin, end);
    C_UNLOCK (gsGlobalRandom);

    return result;
}

cdouble c_random_double (void)
{
    double result;
    C_LOCK (gsGlobalRandom);
    result = c_rand_double (get_global_random ());
    C_UNLOCK (gsGlobalRandom);

    return result;
}

cdouble c_random_double_range (cdouble begin, cdouble end)
{
    double result;
    C_LOCK (gsGlobalRandom);
    result = c_rand_double_range (get_global_random (), begin, end);
    C_UNLOCK (gsGlobalRandom);

    return result;
}
const char* c_getenv (const char* variable)
{
    c_return_val_if_fail (variable != NULL, NULL);

    return getenv (variable);
}

bool c_setenv (const char* variable, const char* value, bool overwrite)
{
    int result;
    char* str;

    c_return_val_if_fail (variable != NULL, false);
    c_return_val_if_fail (strchr (variable, '=') == NULL, false);
    c_return_val_if_fail (value != NULL, false);

    result = setenv (variable, value, overwrite);

    return (result == 0);
}

void c_unsetenv (const char* variable)
{
    c_return_if_fail (variable != NULL);
    c_return_if_fail (strchr (variable, '=') == NULL);

    unsetenv (variable);
}

char** c_listenv (void)
{
    char** result, *eq;
    int len, i, j;

    len = c_strv_length (environ);
    result = c_malloc0(sizeof(char*) * (len + 1));

    j = 0;
    for (i = 0; i < len; i++)
    {
        eq = strchr (environ[i], '=');
        if (eq)
            result[j++] = c_strndup (environ[i], eq - environ[i]);
    }

    result[j] = NULL;

    return result;
}

char** c_get_environ (void)
{
    return c_strdupv ((const char**) environ);
}

const char* c_environ_getenv (char** envp, const char* variable)
{
    int index;

    c_return_val_if_fail (variable != NULL, NULL);

    index = c_environ_find (envp, variable);
    if (index != -1) {
        return envp[index] + strlen (variable) + 1;
    }
    else {
        return NULL;
    }
}

char** c_environ_setenv (char** envp, const char* variable, const char* value, bool overwrite)
{
    int index;

    c_return_val_if_fail (variable != NULL, NULL);
    c_return_val_if_fail (strchr (variable, '=') == NULL, NULL);
    c_return_val_if_fail (value != NULL, NULL);

    index = c_environ_find (envp, variable);
    if (index != -1) {
        if (overwrite) {
            c_free (envp[index]);
            envp[index] = c_strdup_printf ("%s=%s", variable, value);
        }
    }
    else {
        int length;

        length = envp ? (int) c_strv_length (envp) : 0;
        envp = c_realloc(envp, sizeof(char*) * (length + 2));
        envp[length] = c_strdup_printf ("%s=%s", variable, value);
        envp[length + 1] = NULL;
    }

    return envp;
}

char** c_environ_unsetenv (char** envp, const char* variable)
{
    c_return_val_if_fail (variable != NULL, NULL);
    c_return_val_if_fail (strchr (variable, '=') == NULL, NULL);

    if (envp == NULL) {
        return NULL;
    }

    return c_environ_unsetenv_internal (envp, variable, true);
}


int c_strcmp0 (const char* str1, const char* str2)
{
    if (!str1) {
        return -(str1 != str2);
    }
    if (!str2) {
        return str1 != str2;
    }

    return strcmp (str1, str2);
}

void c_clear_pointer (void** pp, CDestroyNotify destroy)
{
    void* _p = *pp;
    if (_p) {
        *pp = NULL;
        destroy (_p);
    }
}

cint64 c_get_monotonic_time (void)
{
    struct timespec ts;
    cint result;

    result = clock_gettime (CLOCK_MONOTONIC, &ts);

    if C_UNLIKELY (result != 0) {
        C_LOG_ERROR_CONSOLE("CLib requires working CLOCK_MONOTONIC");
    }

    return (((cint64) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

cint64 c_get_real_time (void)
{
    struct timeval r;

    /* this is required on alpha, there the timeval structs are ints
     * not longs and a cast only would fail horribly */
    gettimeofday (&r, NULL);

    return (((cint64) r.tv_sec) * 1000000) + r.tv_usec;
}

int c_access (const char* filename, int mode)
{
    return access (filename, mode);
}

int c_chmod (const char* filename, int mode)
{
    return chmod (filename, mode);
}

int c_open (const char* filename, int flags, int mode)
{
    int fd;

    do {
        fd = open (filename, flags, mode);
    }
    while (C_UNLIKELY (fd == -1 && errno == EINTR));

    return fd;
}

int c_creat (const char* filename, int mode)
{
    return creat (filename, mode);
}

int c_rename (const char* oldFileName, const char* newFileName)
{
    return rename (oldFileName, newFileName);
}

int c_mkdir (const char* filename, int mode)
{
    return mkdir (filename, mode);
}

int c_chdir (const char* path)
{
    return chdir (path);
}

int c_stat (const char* filename, CStatBuf* buf)
{
    return stat (filename, buf);
}

int c_lstat (const char* filename, CStatBuf* buf)
{
    return c_stat (filename, buf);
}

int c_unlink (const char* filename)
{
    return unlink (filename);
}

int c_remove (const char* filename)
{
    return remove (filename);
}

int c_rmdir (const char* filename)
{
    return rmdir (filename);
}


FILE* c_fopen (const char* filename, const char* mode)
{
    return fopen (filename, mode);
}

FILE* c_freopen (const char* filename, const char* mode, FILE* stream)
{
    return freopen (filename, mode, stream);
}

cint c_fsync (cint fd)
{
    int retVal;
    do {
        retVal = fsync (fd);
    } while (C_UNLIKELY (retVal < 0 && errno == EINTR));

    return 0;
}

int c_utime (const char* filename, struct utimbuf* utb)
{
    return utime (filename, utb);
}

bool c_close (cint fd, CError** error)
{
    int res;
    res = close (fd);
    if (res == -1) {
        int errsv = errno;
        if (errsv == EINTR) {
            return true;
        }

        if (error) {
            c_set_error_literal (error, C_FILE_ERROR, c_file_error_from_errno (errsv), c_strerror (errsv));
        }

        if (errsv == EBADF) {
            if (fd >= 0) {
                C_LOG_ERROR_CONSOLE("c_close(fd:%d) failed with EBADF. The tracking of file descriptors got messed up", fd);
            }
            else {
                C_LOG_ERROR_CONSOLE("c_close(fd:%d) failed with EBADF. This is not a valid file descriptor", fd);
            }
        }
        errno = errsv;
        return false;
    }

    return true;
}

const char* clib_pgettext (const char* msgCtxTid, csize msgIdOffset)
{
    ensure_gettext_initialized ();

    return c_dpgettext (GETTEXT_PACKAGE, msgCtxTid, msgIdOffset);
}

const char* clib_gettext (const char* str)
{
    ensure_gettext_initialized ();

    return c_dgettext (GETTEXT_PACKAGE, str);
}

const char* c_strip_context (const char* msgId, const char* msgVal)
{
    if (msgVal == msgId) {
        const char *c = strchr (msgId, '|');
        if (c != NULL) {
            return c + 1;
        }
    }

    return msgVal;
}

const char* c_dpgettext (const char* domain, const char* msgCtxTid, csize msgIdOffset)
{
    char* sep = NULL;
    const char* translation = NULL;

    translation = c_dgettext (domain, msgCtxTid);

    if (translation == msgCtxTid) {
        if (msgIdOffset > 0) {
            return msgCtxTid + msgIdOffset;
        }

        sep = strchr (msgCtxTid, '|');
        if (sep) {
            char* tmp = c_malloc0(strlen (msgCtxTid) + 1);
            strcpy (tmp, msgCtxTid);
            tmp[sep - msgCtxTid] = '\004';

            translation = c_dgettext (domain, tmp);
            if (translation == tmp) {
                return sep + 1;
            }
        }
    }

    return translation;
}

const char* c_dpgettext2 (const char* domain, const char* msgCtxt, const char* msgId)
{
    size_t msgCtxtLen = strlen (msgCtxt) + 1;
    size_t msgIdLen = strlen (msgId) + 1;
    const char* translation;
    char* msgCtxtId;

    msgCtxtId = c_malloc0 (msgCtxtLen + msgIdLen);
    memcpy (msgCtxtId, msgCtxt, msgCtxtLen - 1);
    msgCtxtId[msgCtxtLen - 1] = '\004';
    memcpy (msgCtxtId + msgCtxtLen, msgId, msgIdLen);

    translation = c_dgettext (domain, msgCtxtId);

    if (translation == msgCtxtId) {
        msgCtxtId[msgCtxtLen - 1] = '|';
        translation = c_dgettext (domain, msgCtxtId);
        if (translation == msgCtxtId) {
            return msgId;
        }
    }

    return translation;
}

const char* c_dgettext (const char* domain, const char* msgId)
{
    if (domain && C_UNLIKELY (!_c_dgettext_should_translate ())) {
        return msgId;
    }

    return dgettext (domain, msgId);
}

const char* c_dcgettext (const char* domain, const char* msgId, cint category)
{
    if (domain && C_UNLIKELY (!_c_dgettext_should_translate ())) {
        return msgId;
    }

    return dcgettext (domain, msgId, category);
}

const char* c_dngettext (const char* domain, const char* msgId, const char* msgIdPlural, culong n)
{
    if (domain && C_UNLIKELY (!_c_dgettext_should_translate ())) {
        return n == 1 ? msgId : msgIdPlural;
    }

    return dngettext (domain, msgId, msgIdPlural, n);
}

bool c_is_power_of_2(cuint64 value)
{
    return (value!= 0) && ((value & (value - 1)) == 0);
}


static void msort_r (void *b, cuint64 n, cuint64 s, CCompareDataFunc cmp, void *arg)
{
    MSortParam p;
    cuint64 size = n * s;

    /* For large object sizes use indirect sorting.  */
    if (s > 32) {
        size = 2 * n * sizeof (void *) + s;
    }

    char* tmp = c_malloc0 (size);
    p.t = tmp;

    p.s = s;
    p.var = 4;
    p.cmp = cmp;
    p.arg = arg;

    if (s > 32) {
        /* Indirect sorting.  */
        char *ip = (char *) b;
        void **tp = (void **) (p.t + n * sizeof (void *));
        void **t = tp;
        void *tmp_storage = (void *) (tp + n);
        char *kp;
        size_t i;

        while ((void *) t < tmp_storage) {
            *t++ = ip;
            ip += s;
        }
        p.s = sizeof (void *);
        p.var = 3;
        msort_with_tmp (&p, p.t + n * sizeof (void *), n);

        for (i = 0, ip = (char *) b; i < n; i++, ip += s) {
            if ((kp = tp[i]) != ip) {
                size_t j = i;
                char *jp = ip;
                memcpy (tmp_storage, ip, s);
                do {
                    size_t k = (kp - (char *) b) / s;
                    tp[j] = jp;
                    memcpy (jp, kp, s);
                    j = k;
                    jp = kp;
                    kp = tp[k];
                } while (kp != ip);

                tp[j] = jp;
                memcpy (jp, tmp_storage, s);
            }
        }
    }
    else {
        if (((s & (sizeof (cuint32) - 1)) == 0) && ((cuint64) b % ALIGNOF_CUINT32 == 0)) {
            if (s == sizeof (cuint32)) {
                p.var = 0;
            }
            else if ((s == sizeof (cuint64)) && ((cuint64) b % ALIGNOF_CUINT64 == 0)) {
                p.var = 1;
            }
            else if (((s & (sizeof (unsigned long) - 1)) == 0) && ((cuint64) b % ALIGNOF_UNSIGNED_LONG == 0)) {
                p.var = 2;
            }
        }
        msort_with_tmp (&p, b, n);
    }
    c_free (tmp);
}

static void msort_with_tmp (const MSortParam* p, void *b, size_t n)
{
    char *b1, *b2;
    size_t n1, n2;
    char *tmp = p->t;
    const size_t s = p->s;
    CCompareDataFunc cmp = p->cmp;
    void *arg = p->arg;

    if (n <= 1) {
        return;
    }

    n1 = n / 2;
    n2 = n - n1;
    b1 = b;
    b2 = (char *) b + (n1 * p->s);

    msort_with_tmp (p, b1, n1);
    msort_with_tmp (p, b2, n2);

    switch (p->var) {
        case 0: {
            while (n1 > 0 && n2 > 0) {
                if ((*cmp) (b1, b2, arg) <= 0) {
                    *(cuint32 *) tmp = *(cuint32 *) b1;
                    b1 += sizeof (cuint32);
                    --n1;
                }
                else {
                    *(cuint32 *) tmp = *(cuint32 *) b2;
                    b2 += sizeof (cuint32);
                    --n2;
                }
                tmp += sizeof (cuint32);
            }
            break;
        }
        case 1: {
            while (n1 > 0 && n2 > 0) {
                if ((*cmp) (b1, b2, arg) <= 0) {
                    *(cuint64*) tmp = *(cuint64*) b1;
                    b1 += sizeof (cuint64);
                    --n1;
                }
                else {
                    *(cuint64*) tmp = *(cuint64*) b2;
                    b2 += sizeof (cuint64);
                    --n2;
                }
                tmp += sizeof (cuint64);
            }
            break;
        }
        case 2: {
            while (n1 > 0 && n2 > 0) {
                unsigned long *tmpl = (unsigned long *) tmp;
                unsigned long *bl;
                tmp += s;
                if ((*cmp) (b1, b2, arg) <= 0) {
                    bl = (unsigned long *) b1;
                    b1 += s;
                    --n1;
                }
                else {
                    bl = (unsigned long *) b2;
                    b2 += s;
                    --n2;
                }
                while (tmpl < (unsigned long *) tmp) {
                    *tmpl++ = *bl++;
                }
            }
            break;
        }
        case 3: {
            while (n1 > 0 && n2 > 0) {
                if ((*cmp) ((void*) *(const void **) b1, (void*) *(const void **) b2, arg) <= 0) {
                    *(void **) tmp = *(void **) b1;
                    b1 += sizeof (void *);
                    --n1;
                }
                else {
                    *(void **) tmp = *(void **) b2;
                    b2 += sizeof (void *);
                    --n2;
                }
                tmp += sizeof (void *);
            }
            break;
        }
        default: {
            while (n1 > 0 && n2 > 0) {
                if ((*cmp) (b1, b2, arg) <= 0) {
                    memcpy (tmp, b1, s);
                    tmp += s;
                    b1 += s;
                    --n1;
                }
                else {
                    memcpy (tmp, b2, s);
                    tmp += s;
                    b2 += s;
                    --n2;
                }
            }
            break;
        }
    }

    if (n1 > 0) {
        memcpy (tmp, b1, n1 * s);
    }

    memcpy (b, p->t, (n - n2) * s);
}

static cuint get_random_version (void)
{
    static csize initialized = false;
    static cuint randomVersion;

    if (c_once_init_enter (&initialized)) {
        const char* versionString = c_getenv ("C_RANDOM_VERSION");
        if (!versionString || versionString[0] == '\000' || strcmp (versionString, "2.2") == 0) {
            randomVersion = 22;
        }
        else if (strcmp (versionString, "2.0") == 0) {
            randomVersion = 20;
        }
        else {
            C_LOG_ERROR_CONSOLE("Unknown G_RANDOM_VERSION \"%s\". Using version 2.2.", versionString);
            randomVersion = 22;
        }
        c_once_init_leave (&initialized, true);
    }

    return randomVersion;
}

static CRand* get_global_random (void)
{
    static CRand* globalRandom;

    /* called while locked */
    if (!globalRandom) {
        globalRandom = c_rand_new ();
    }

    return globalRandom;
}

static bool c_environ_matches (const char* env, const char* variable, csize len)
{
    return strncmp (env, variable, len) == 0 && env[len] == '=';
}

static int c_environ_find (char** envp, const char* variable)
{
    csize len;
    int i;

    if (envp == NULL)
        return -1;

    len = strlen (variable);

    for (i = 0; envp[i]; i++) {
        if (c_environ_matches (envp[i], variable, len)) {
            return i;
        }
    }

    return -1;
}

static char** c_environ_unsetenv_internal (char** envp, const char* variable, bool freeValue)
{
    csize len;
    char **e, **f;

    len = strlen (variable);

    /* Note that we remove *all* environment entries for
     * the variable name, not just the first.
     */
    e = f = envp;
    while (*e != NULL) {
        if (!c_environ_matches (*e, variable, len)) {
            *f = *e;
            f++;
        }
        else {
            if (freeValue) {
                c_free (*e);
            }
        }

        e++;
    }
    *f = NULL;

    return envp;
}

static void ensure_gettext_initialized (void)
{
    static csize initialised;

    if (c_once_init_enter (&initialised)) {
        bindtextdomain (GETTEXT_PACKAGE, CLIB_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        c_once_init_leave (&initialised, true);
    }
}

static bool _c_dgettext_should_translate (void)
{
    static csize translate = 0;
    enum {
        SHOULD_TRANSLATE = 1,
        SHOULD_NOT_TRANSLATE = 2
    };

    if (C_UNLIKELY (c_once_init_enter (&translate))) {
        bool shouldTranslate = true;

        const char* defaultDomain     = textdomain (NULL);
        const char* translatorComment = gettext ("");
        const char* translateLocale   = setlocale (LC_MESSAGES, NULL);

        if (!defaultDomain
            || !translatorComment
            || !translateLocale
            || ((0 != strcmp (defaultDomain, "messages"))
                && ('\0' == *translatorComment)
                && (0 != strncmp (translateLocale, "en_", 3))
                && (0 != strcmp (translateLocale, "C")))) {
            shouldTranslate = false;
        }
        c_once_init_leave (&translate, shouldTranslate ? SHOULD_TRANSLATE : SHOULD_NOT_TRANSLATE);
    }

    return (translate == SHOULD_TRANSLATE);
}


