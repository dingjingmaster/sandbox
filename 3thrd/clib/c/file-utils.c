
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

#include "file-utils.h"

#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "str.h"
#include "log.h"
#include "clib.h"
#include "quark.h"
#include "error.h"
#include "utils.h"
#include "cstring.h"

#if defined(MAXPATHLEN)
#define C_PATH_LENGTH MAXPATHLEN
#elif defined(PATH_MAX)
#define C_PATH_LENGTH PATH_MAX
#elif defined(_PC_PATH_MAX)
#define C_PATH_LENGTH sysconf(_PC_PATH_MAX)
#else
#define C_PATH_LENGTH 2048
#endif

typedef cint (*CTmpFileCallback) (const char*, cint, cint);


static void arr_move_left1(cuint64 startPos, char* buf);
static cint wrap_g_open (const char* filename, int flags, int mode);
static cint wrap_g_mkdir (const char* filename, int flags C_UNUSED, int mode);
static int get_tmp_file (char* tmpl, CTmpFileCallback f, int flags, int mode);
static char* c_build_filename_va (const char* firstArgument, va_list* args, char** strArray);
static bool fd_should_be_fsynced (int fd, const char* test_file, CFileSetContentsFlags flags);
static char* g_build_filename_va (const char* first_argument, va_list* args, char** str_array);
static bool rename_file (const char* old_name, const char* new_name, bool do_fsync, CError** err);
static bool get_contents_posix (const char* filename, char** contents, csize* length, CError** error);
static void set_file_error (CError** error, const char* filename, const char* format_string, int saved_errno);
static char* c_build_path_va (const char* separator, const char* firstElement, va_list* args, char** strArray);
static bool get_contents_stdio (const char* filename, FILE* f, char** contents, csize* length, CError** error);
static char* g_build_path_va (const char* separator, const char* first_element, va_list* args, char** str_array);
static int g_get_tmp_name (const char* tmpl, char** name_used, CTmpFileCallback f, cint flags, cint mode, CError** error);
static bool write_to_file (const char* contents, csize length, int fd, const char* dest_file, bool do_fsync, CError** err);
static bool get_contents_regfile (const char* filename, struct stat* stat_buf, cint fd, char** contents, csize* length, CError** error);


C_DEFINE_QUARK(c-file-error-quark, c_file_error)


static char *format_error_message (const char* filename, const char* formatString, int savedErrno) C_FORMAT(2);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static char * format_error_message (const char* filename, const char* format_string, int saved_errno)
{
    char *display_name;
    char *msg;

    display_name = c_filename_display_name (filename);
    msg = c_strdup_printf (format_string, display_name, c_strerror (saved_errno));
    c_free (display_name);

    return msg;
}
#pragma GCC diagnostic pop


bool c_file_test (const char* fileName, CFileTest test)
{
    c_return_val_if_fail (fileName != NULL, false);

    if ((test & C_FILE_TEST_EXISTS) && (access (fileName, F_OK) == 0)) {
        return true;
    }

    if ((test & C_FILE_TEST_IS_EXECUTABLE) && (access (fileName, X_OK) == 0)) {
        if (getuid () != 0) {
            return true;
        }
    }
    else {
        test &= ~C_FILE_TEST_IS_EXECUTABLE;
    }

    if (test & C_FILE_TEST_IS_SYMLINK) {
        struct stat s;
        if ((lstat (fileName, &s) == 0) && S_ISLNK (s.st_mode)) {
            return true;
        }
    }

    if (test & (C_FILE_TEST_IS_REGULAR | C_FILE_TEST_IS_DIR | C_FILE_TEST_IS_EXECUTABLE)) {
        struct stat s;
        if (stat (fileName, &s) == 0) {
            if ((test & C_FILE_TEST_IS_REGULAR) && S_ISREG (s.st_mode)) {
                return true;
            }

            if ((test & C_FILE_TEST_IS_DIR) && S_ISDIR (s.st_mode)) {
                return true;
            }

            if ((test & C_FILE_TEST_IS_EXECUTABLE) && ((s.st_mode & S_IXOTH) || (s.st_mode & S_IXUSR) || (s.st_mode & S_IXGRP))) {
                return true;
            }
        }
    }

    return false;
}

char* c_build_filename (const char* firstElement, ...)
{
    va_list args;

    va_start (args, firstElement);
    char* str = c_build_filename_va (firstElement, &args, NULL);
    va_end (args);

    return str;
}

char* c_build_filenamev (char** args)
{
    return c_build_filename_va (NULL, NULL, args);
}

char* c_build_filename_valist (const char* firstElement, va_list* args)
{
    c_return_val_if_fail (firstElement != NULL, NULL);

    return c_build_filename_va (firstElement, args, NULL);
}

bool c_path_is_absolute (const char* fileName)
{
    c_return_val_if_fail (fileName != NULL, false);

    if (C_IS_DIR_SEPARATOR (fileName[0])) {
        return true;
    }

    return false;
}

int c_mkdir_with_parents(const char *pathname, cint mode)
{
    char *fn, *p;

    if (pathname == NULL || *pathname == '\0') {
        errno = EINVAL;
        return -1;
    }

    /* try to create the full path first */
    if (c_mkdir (pathname, mode) == 0) {
        return 0;
    }
    else if (errno == EEXIST) {
        if (!c_file_test (pathname, C_FILE_TEST_IS_DIR)) {
            errno = ENOTDIR;
            return -1;
        }
        return 0;
    }

    /* walk the full path and try creating each element */
    fn = c_strdup (pathname);

    if (c_path_is_absolute (fn)) {
        p = (char*) c_path_skip_root(fn);
    }
    else {
        p = fn;
    }

    do {
        while (*p && !C_IS_DIR_SEPARATOR (*p)) {
            p++;
        }

        if (!*p) {
            p = NULL;
        }
        else {
            *p = '\0';
        }

        if (!c_file_test (fn, C_FILE_TEST_EXISTS)) {
            if (c_mkdir (fn, mode) == -1 && errno != EEXIST) {
                int errno_save = errno;
                if (errno != ENOENT || !p) {
                    c_free (fn);
                    errno = errno_save;
                    return -1;
                }
            }
        }
        else if (!c_file_test (fn, C_FILE_TEST_IS_DIR)) {
            c_free (fn);
            errno = ENOTDIR;
            return -1;
        }

        if (p) {
            *p++ = C_DIR_SEPARATOR;
            while (*p && C_IS_DIR_SEPARATOR (*p)) {
                p++;
            }
        }
    }
    while (p);

    c_free (fn);

    return 0;
}

CFileError c_file_error_from_errno(int errNo)
{
    switch (errNo)
    {
#ifdef EEXIST
        case EEXIST:
            return C_FILE_ERROR_EXIST;
#endif

#ifdef EISDIR
        case EISDIR:
            return C_FILE_ERROR_ISDIR;
#endif

#ifdef EACCES
        case EACCES:
            return C_FILE_ERROR_ACCES;
#endif

#ifdef ENAMETOOLONG
        case ENAMETOOLONG:
            return C_FILE_ERROR_NAMETOOLONG;
#endif

#ifdef ENOENT
        case ENOENT:
            return C_FILE_ERROR_NOENT;
#endif

#ifdef ENOTDIR
        case ENOTDIR:
            return C_FILE_ERROR_NOTDIR;
#endif

#ifdef ENXIO
        case ENXIO:
            return C_FILE_ERROR_NXIO;
#endif

#ifdef ENODEV
        case ENODEV:
            return C_FILE_ERROR_NODEV;
#endif

#ifdef EROFS
        case EROFS:
            return C_FILE_ERROR_ROFS;
#endif

#ifdef ETXTBSY
        case ETXTBSY:
            return C_FILE_ERROR_TXTBSY;
#endif

#ifdef EFAULT
        case EFAULT:
            return C_FILE_ERROR_FAULT;
#endif

#ifdef ELOOP
        case ELOOP:
            return C_FILE_ERROR_LOOP;
#endif

#ifdef ENOSPC
        case ENOSPC:
            return C_FILE_ERROR_NOSPC;
#endif

#ifdef ENOMEM
        case ENOMEM:
            return C_FILE_ERROR_NOMEM;
#endif

#ifdef EMFILE
        case EMFILE:
            return C_FILE_ERROR_MFILE;
#endif

#ifdef ENFILE
        case ENFILE:
            return C_FILE_ERROR_NFILE;
#endif

#ifdef EBADF
        case EBADF:
            return C_FILE_ERROR_BADF;
#endif

#ifdef EINVAL
        case EINVAL:
            return C_FILE_ERROR_INVAL;
#endif

#ifdef EPIPE
        case EPIPE:
            return C_FILE_ERROR_PIPE;
#endif

#ifdef EAGAIN
        case EAGAIN:
            return C_FILE_ERROR_AGAIN;
#endif

#ifdef EINTR
        case EINTR:
            return C_FILE_ERROR_INTR;
#endif

#ifdef EIO
        case EIO:
            return C_FILE_ERROR_IO;
#endif

#ifdef EPERM
        case EPERM:
            return C_FILE_ERROR_PERM;
#endif

#ifdef ENOSYS
        case ENOSYS:
            return C_FILE_ERROR_NOSYS;
#endif

        default:
            break;
    }

    return C_FILE_ERROR_FAILED;
}

bool c_file_get_contents(const char *filename, char **contents, csize *length, CError **error)
{
    c_return_val_if_fail (filename != NULL, false);
    c_return_val_if_fail (contents != NULL, false);

    *contents = NULL;
    if (length) {
        *length = 0;
    }

#ifdef G_OS_WIN32
    //return get_contents_win32 (filename, contents, length, error);
#else
    return get_contents_posix (filename, contents, length, error);
#endif
}

bool c_file_set_contents(const char *filename, const char *contents, cssize length, CError **error)
{
    return c_file_set_contents_full (filename, contents, length, C_FILE_SET_CONTENTS_CONSISTENT | C_FILE_SET_CONTENTS_ONLY_EXISTING, 0666, error);
}

bool c_file_set_contents_full (const char* filename, const char* contents, cssize length, CFileSetContentsFlags flags, int mode, CError** error)
{
    c_return_val_if_fail (filename != NULL, false);
    c_return_val_if_fail (error == NULL || *error == NULL, false);
    c_return_val_if_fail (contents != NULL || length == 0, false);
    c_return_val_if_fail (length >= -1, false);

    /* @flags are handled as follows:
     *  - %G_FILE_SET_CONTENTS_NONE: write directly to @filename, no fsync()s
     *  - %G_FILE_SET_CONTENTS_CONSISTENT: write to temp file, fsync() it, rename()
     *  - %G_FILE_SET_CONTENTS_CONSISTENT | ONLY_EXISTING: as above, but skip the
     *    fsync() if @filename doesn’t exist or is empty
     *  - %G_FILE_SET_CONTENTS_DURABLE: write directly to @filename, fsync() it
     *  - %G_FILE_SET_CONTENTS_DURABLE | ONLY_EXISTING: as above, but skip the
     *    fsync() if @filename doesn’t exist or is empty
     *  - %G_FILE_SET_CONTENTS_CONSISTENT | DURABLE: write to temp file, fsync()
     *    it, rename(), fsync() containing directory
     *  - %G_FILE_SET_CONTENTS_CONSISTENT | DURABLE | ONLY_EXISTING: as above, but
     *    skip both fsync()s if @filename doesn’t exist or is empty
     */

    if (length < 0) {
        length = strlen (contents);
    }

    if (flags & C_FILE_SET_CONTENTS_CONSISTENT) {
        char *tmp_filename = NULL;
        CError *rename_error = NULL;
        bool retval;
        int fd;
        bool do_fsync;

        tmp_filename = c_strdup_printf ("%s.XXXXXX", filename);

        errno = 0;
        fd = c_mkstemp_full (tmp_filename, O_RDWR, mode);

        if (fd == -1) {
            int saved_errno = errno;
            if (error)
                set_file_error (error, tmp_filename, _("Failed to create file “%s”: %s"), saved_errno);
            retval = false;
            goto consistent_out;
        }

        do_fsync = fd_should_be_fsynced (fd, filename, flags);
        if (!write_to_file (contents, length, c_steal_fd (&fd), tmp_filename, do_fsync, error)) {
            c_unlink (tmp_filename);
            retval = false;
            goto consistent_out;
        }

        if (!rename_file (tmp_filename, filename, do_fsync, &rename_error)) {
#ifndef G_OS_WIN32
            c_unlink (tmp_filename);
            c_propagate_error (error, rename_error);
            retval = false;
            goto consistent_out;

#else /* G_OS_WIN32 */

            /* Renaming failed, but on Windows this may just mean
           * the file already exists. So if the target file
           * exists, try deleting it and do the rename again.
           */
            if (!c_file_test (filename, C_FILE_TEST_EXISTS)) {
                c_unlink (tmp_filename);
                c_propagate_error (error, rename_error);
                retval = false;
                goto consistent_out;
            }

            c_error_free (rename_error);

            if (c_unlink (filename) == -1) {
                int saved_errno = errno;
                if (error)
                    set_file_error (error,
                                filename,
                                _("Existing file “%s” could not be removed: g_unlink() failed: %s"),
                                saved_errno);
                c_unlink (tmp_filename);
                retval = false;
                goto consistent_out;
            }

            if (!rename_file (tmp_filename, filename, flags, error)) {
                c_unlink (tmp_filename);
                retval = false;
                goto consistent_out;
            }

#endif  /* G_OS_WIN32 */
        }
        retval = true;

consistent_out:
        c_free (tmp_filename);
        return retval;
    }
    else {
        int direct_fd;
        int open_flags;
        bool do_fsync;

        open_flags = O_RDWR | O_CREAT | O_CLOEXEC;
#ifdef O_NOFOLLOW
        /* Windows doesn’t have symlinks, so O_NOFOLLOW is unnecessary there. */
        open_flags |= O_NOFOLLOW;
#endif

        errno = 0;
        direct_fd = c_open (filename, open_flags, mode);
        if (direct_fd < 0) {
            int saved_errno = errno;

#ifdef O_NOFOLLOW
            /* ELOOP indicates that @filename is a symlink, since we used
             * O_NOFOLLOW (alternately it could indicate that @filename contains
             * looping or too many symlinks). In either case, try again on the
             * %G_FILE_SET_CONTENTS_CONSISTENT code path.
             *
             * FreeBSD uses EMLINK instead of ELOOP
             * (https://www.freebsd.org/cgi/man.cgi?query=open&sektion=2#STANDARDS),
             * and NetBSD uses EFTYPE
             * (https://netbsd.gw.com/cgi-bin/man-cgi?open+2+NetBSD-current). */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
            if (saved_errno == EMLINK)
#elif defined(__NetBSD__)
            if (saved_errno == EFTYPE)
#else
            if (saved_errno == ELOOP)
#endif
                return c_file_set_contents_full (filename, contents, length, flags | C_FILE_SET_CONTENTS_CONSISTENT, mode, error);
#endif  /* O_NOFOLLOW */

            if (error) {
                set_file_error (error, filename, _("Failed to open file “%s”: %s"), saved_errno);
            }
            return false;
        }

        do_fsync = fd_should_be_fsynced (direct_fd, filename, flags);
        if (!write_to_file (contents, length, c_steal_fd (&direct_fd), filename, do_fsync, error)) {
            return false;
        }
    }

    return true;
}

char *c_mkdtemp_full(char *tmpl, int mode)
{
    if (get_tmp_file (tmpl, wrap_g_mkdir, 0, mode) == -1)
        return NULL;
    else
        return tmpl;

    return NULL;
}

char *c_mkdtemp(char *tmpl)
{
    return c_mkdtemp_full (tmpl, 0700);
}

cint c_mkstemp_full(char *tmpl, cint flags, cint mode)
{
    return get_tmp_file (tmpl, wrap_g_open, flags | O_CREAT | O_EXCL, mode);
}

cint c_mkstemp(char *tmpl)
{
    return c_mkstemp_full (tmpl, O_RDWR, 0600);
}

cint c_file_open_tmp(const char *tmpl, char **nameUsed, CError **error)
{
    char *fulltemplate;
    int result;

    c_return_val_if_fail (error == NULL || *error == NULL, -1);

    result = g_get_tmp_name (tmpl, &fulltemplate,
                             wrap_g_open,
                             O_CREAT | O_EXCL | O_RDWR,
                             0600,
                             error);
    if (result != -1) {
        if (nameUsed) {
            *nameUsed = fulltemplate;
        }
        else {
            c_free (fulltemplate);
        }
    }

    return result;
}

char *c_dir_make_tmp(const char *tmpl, CError **error)
{
    char *fulltemplate;

    c_return_val_if_fail (error == NULL || *error == NULL, NULL);

    if (g_get_tmp_name (tmpl, &fulltemplate, wrap_g_mkdir, 0, 0700, error) == -1) {
        return NULL;
    }
    else {
        return fulltemplate;
    }
}

char *c_build_pathv(const char *separator, char **args)
{
    if (!args) {
        return NULL;
    }

    return c_build_path_va (separator, NULL, NULL, args);
}

char *c_build_path(const char *separator, const char *firstElement, ...)
{
    char *str;
    va_list args;

    c_return_val_if_fail (separator != NULL, NULL);

    va_start (args, firstElement);
    str = g_build_path_va (separator, firstElement, &args, NULL);
    va_end (args);

    return str;
}

char *c_file_read_link(const char *filename, CError **error)
{
    char *buffer;
    size_t size;
    cssize read_size;

    c_return_val_if_fail (filename != NULL, NULL);
    c_return_val_if_fail (error == NULL || *error == NULL, NULL);

    size = 256;
    buffer = c_malloc0 (size);

    while (true) {
        read_size = readlink (filename, buffer, size);
        if (read_size < 0) {
            int saved_errno = errno;
            if (error)
                set_file_error (error,
                            filename,
                            _("Failed to read the symbolic link “%s”: %s"),
                            saved_errno);
            c_free (buffer);
            return NULL;
        }

        if ((size_t) read_size < size) {
            buffer[read_size] = 0;
            return buffer;
        }

        size *= 2;
        buffer = c_realloc (buffer, size);
    }
#if defined (G_OS_WIN32)
    char *buffer;
    cssize read_size;

    c_return_val_if_fail (filename != NULL, NULL);
    c_return_val_if_fail (error == NULL || *error == NULL, NULL);

    read_size = c_win32_readlink_utf8 (filename, NULL, 0, &buffer, TRUE);
    if (read_size < 0) {
        int saved_errno = errno;
        if (error)
            set_file_error (error,
                        filename,
                        _("Failed to read the symbolic link “%s”: %s"),
                        saved_errno);
        return NULL;
    }
    else if (read_size == 0) {
        return strdup ("");
    }
    else {
        return buffer;
    }
#endif

    return NULL;
}

const char *c_path_skip_root(const char *fileName)
{
    c_return_val_if_fail (fileName != NULL, NULL);

    /* Skip initial slashes */
    if (C_IS_DIR_SEPARATOR (fileName[0])) {
        while (C_IS_DIR_SEPARATOR (fileName[0])) {
            fileName++;
        }
        return (char*)fileName;
    }

    return NULL;
}

const char *c_basename(const char *fileName)
{
    char *base;

    c_return_val_if_fail (fileName != NULL, NULL);

    base = strrchr (fileName, C_DIR_SEPARATOR);
    if (base) {
        return base + 1;
    }

    return (char*) fileName;
}

char *c_path_get_basename(const char *fileName)
{
    cssize base;
    cssize last_nonslash;
    csize len;
    char *retval;

    c_return_val_if_fail (fileName != NULL, NULL);

    if (fileName[0] == '\0') {
        return c_strdup (".");
    }

    last_nonslash = strlen (fileName) - 1;

    while (last_nonslash >= 0 && C_IS_DIR_SEPARATOR (fileName [last_nonslash])) {
        last_nonslash--;
    }

    if (last_nonslash == -1) {
        /* string only containing slashes */
        return c_strdup (C_DIR_SEPARATOR_S);
    }

    base = last_nonslash;
    while (base >=0 && !C_IS_DIR_SEPARATOR (fileName [base])) {
        base--;
    }

    len = last_nonslash - base;
    retval = c_malloc0 (len + 1);
    memcpy (retval, fileName + (base + 1), len);
    retval [len] = '\0';

    return retval;
}

char *c_path_get_dirname(const char *fileName)
{
    char *base;
    csize len;

    c_return_val_if_fail (fileName != NULL, NULL);

    base = strrchr (fileName, C_DIR_SEPARATOR);
    if (!base) {
        return c_strdup (".");
    }

    while (base > fileName && C_IS_DIR_SEPARATOR (*base)) {
        base--;
    }

    len = (cuint) 1 + base - fileName;
    base = c_malloc0(sizeof(char) * (len + 1));
    memmove (base, fileName, len);
    base[len] = 0;

    return base;
}

char *c_canonicalize_filename(const char *filename, const char *relativeTo)
{
    char *canon, *input, *output, *after_root, *output_start;

    c_return_val_if_fail (relativeTo == NULL || c_path_is_absolute (relativeTo), NULL);

    if (!c_path_is_absolute (filename)) {
        char *cwd_allocated = NULL;
        const char  *cwd;
        if (relativeTo != NULL)
            cwd = relativeTo;
        else
            cwd = cwd_allocated = c_get_current_dir ();

        canon = c_build_filename (cwd, filename, NULL);
        c_free (cwd_allocated);
    }
    else {
        canon = c_strdup (filename);
    }

    after_root = (char*) c_path_skip_root (canon);

    if (after_root == NULL) {
        c_free (canon);
        return c_build_filename (C_DIR_SEPARATOR_S, filename, NULL);
    }

    /* Find the first dir separator and use the canonical dir separator. */
    for (output = after_root - 1; (output >= canon) && C_IS_DIR_SEPARATOR (*output); output--)
        *output = C_DIR_SEPARATOR;

    /* 1 to re-increment after the final decrement above (so that output >= canon),
     * and 1 to skip the first `/`. There might not be a first `/` if
     * the @canon is a Windows `//server/share` style path with no
     * trailing directories. @after_root will be '\0' in that case. */
    output++;
    if (*output == C_DIR_SEPARATOR)
        output++;

    /* POSIX allows double slashes at the start to mean something special
     * (as does windows too). So, "//" != "/", but more than two slashes
     * is treated as "/".
     */
    if (after_root - output == 1)
        output++;

    input = after_root;
    output_start = output;
    while (*input) {
        /* input points to the next non-separator to be processed. */
        /* output points to the next location to write to. */
        c_assert (input > canon && C_IS_DIR_SEPARATOR (input[-1]));
        c_assert (output > canon && C_IS_DIR_SEPARATOR (output[-1]));
        c_assert (input >= output);

        /* Ignore repeated dir separators. */
        while (C_IS_DIR_SEPARATOR (input[0]))
            input++;

        /* Ignore single dot directory components. */
        if (input[0] == '.' && (input[1] == 0 || C_IS_DIR_SEPARATOR (input[1]))) {
            if (input[1] == 0)
                break;
            input += 2;
        }
            /* Remove double-dot directory components along with the preceding
             * path component. */
        else if (input[0] == '.' && input[1] == '.' && (input[2] == 0 || C_IS_DIR_SEPARATOR (input[2]))) {
            if (output > output_start) {
                do {
                    output--;
                }
                while (!C_IS_DIR_SEPARATOR (output[-1]) && output > output_start);
            }
            if (input[2] == 0)
                break;
            input += 3;
        }
        else {
            while (*input && !C_IS_DIR_SEPARATOR (*input))
                *output++ = *input++;
            if (input[0] == 0)
                break;
            input++;
            *output++ = C_DIR_SEPARATOR;
        }
    }

    /* Remove a potentially trailing dir separator */
    if (output > output_start && C_IS_DIR_SEPARATOR (output[-1]))
        output--;

    *output = '\0';

    return canon;
}

cuint64 c_file_read_line_arr(FILE* fr, char lineBuf[], cuint64 bufLen)
{
    cuint64 len = 0;

    c_warn_if_fail(fr && lineBuf && bufLen > 0);
    c_return_val_if_fail (fr != NULL, 0);
    c_return_val_if_fail (bufLen > 0, 0);
    c_return_val_if_fail (lineBuf != NULL, 0);

    while (len < bufLen - 1) {
        cchar c = fgetc (fr);
        if (c == EOF) {
            break;
        }
        lineBuf[len++] = c;
        if (c == '\n') {
            break;
        }
    }
    lineBuf[len] = '\0';

    return len;
}

char * c_file_path_format_arr(char pathBuf[])
{
    c_return_val_if_fail(pathBuf, NULL);

    if ('/' == pathBuf[0]) {
        // 去掉字符串中 "//" 类似字符串，变为 /
        int idx = 0;
        for (idx = 1; '\0' != pathBuf[idx]; idx++) {
            if (('/' == pathBuf[idx]) && ('/' == pathBuf[idx - 1])) {
                arr_move_left1(idx, pathBuf);
                idx--;
            }
        }
        if (c_str_has_suffix(pathBuf, "/")) {
            pathBuf[c_strlen(pathBuf) - 1] = '\0';
        }
    }
    // TODO://

    return pathBuf;
}


char *c_get_current_dir(void)
{
    const char *pwd;
    char *buffer = NULL;
    char *dir = NULL;
    static culong max_len = 0;
    struct stat pwdbuf, dotbuf;

    pwd = c_getenv ("PWD");
    if (pwd != NULL &&
        c_stat (".", &dotbuf) == 0 && c_stat (pwd, &pwdbuf) == 0 &&
        dotbuf.st_dev == pwdbuf.st_dev && dotbuf.st_ino == pwdbuf.st_ino)
        return c_strdup (pwd);

    if (max_len == 0)
        max_len = (C_PATH_LENGTH == -1) ? 2048 : C_PATH_LENGTH;

    while (max_len < C_MAX_ULONG / 2) {
        c_free (buffer);
        buffer = c_malloc0(sizeof(char) * (max_len + 1));
        *buffer = 0;
        dir = getcwd (buffer, max_len);

        if (dir || errno != ERANGE)
            break;

        max_len *= 2;
    }

    if (!dir || !*buffer) {
        buffer[0] = C_DIR_SEPARATOR;
        buffer[1] = 0;
    }

    dir = c_strdup (buffer);
    c_free (buffer);

    return dir;
}

static bool rename_file (const char* old_name, const char* new_name, bool do_fsync, CError** err)
{
    errno = 0;
    if (c_rename (old_name, new_name) == -1) {
        int save_errno = errno;
        char *display_old_name = c_filename_display_name (old_name);
        char *display_new_name = c_filename_display_name (new_name);

        c_set_error (err, C_FILE_ERROR, c_file_error_from_errno (save_errno),
                        _("Failed to rename file “%s” to “%s”: g_rename() failed: %s"),
                        display_old_name,
                        display_new_name,
                        c_strerror (save_errno));

        c_free (display_old_name);
        c_free (display_new_name);

        return false;
    }

    /* In order to guarantee that the *new* contents of the file are seen in
     * future, fsync() the directory containing the file. Otherwise if the file
     * system was unmounted cleanly now, it would be undefined whether the old
     * or new contents of the file were visible after recovery.
     *
     * This assumes the @old_name and @new_name are in the same directory. */
#ifdef HAVE_FSYNC
    if (do_fsync) {
        char *dir = c_path_get_dirname (new_name);
        int dir_fd = c_open (dir, O_RDONLY, 0);

        if (dir_fd >= 0) {
            c_fsync (dir_fd);
            c_close (dir_fd, NULL);
        }

        c_free (dir);
    }
#endif  /* HAVE_FSYNC */

    return true;
}

static char* c_build_filename_va (const char* firstArgument, va_list* args, char** strArray)
{
    char* str = c_build_path_va (C_DIR_SEPARATOR_S, firstArgument, args, strArray);

    return str;
}

static char* c_build_path_va (const char* separator, const char* firstElement, va_list* args, char** strArray)
{
    CString *result;
    int separatorLen = (int) strlen (separator);
    bool isFirst = true;
    bool haveLeading = false;
    const char* singleElement = NULL;
    const char* nextElement;
    const char* lastTrailing = NULL;
    int i = 0;

    result = c_string_new (NULL);

    if (strArray) {
        nextElement = strArray[i++];
    }
    else {
        nextElement = firstElement;
    }

    while (true) {
        const char* element;
        const char* start;
        const char* end;

        if (nextElement) {
            element = nextElement;
            if (strArray) {
                nextElement = strArray[i++];
            }
            else {
                nextElement = va_arg (*args, char*);
            }
        }
        else {
            break;
        }

        if (!*element) {
            continue;
        }

        start = element;
        if (separatorLen) {
            while (strncmp (start, separator, separatorLen) == 0) {
                start += separatorLen;
            }
        }

        end = start + strlen (start);
        if (separatorLen) {
            while (end >= start + separatorLen && strncmp (end - separatorLen, separator, separatorLen) == 0) {
                end -= separatorLen;
            }

            lastTrailing = end;
            while (lastTrailing >= element + separatorLen && strncmp (lastTrailing - separatorLen, separator, separatorLen) == 0) {
                lastTrailing -= separatorLen;
            }

            if (!haveLeading) {
                if (lastTrailing <= start) {
                    singleElement = element;
                }

                c_string_append_len (result, element, start - element);
                haveLeading = true;
            }
            else {
                singleElement = NULL;
            }
        }

        if (end == start) {
            continue;
        }

        if (!isFirst) {
            c_string_append (result, separator);
        }

        c_string_append_len (result, start, end - start);
        isFirst = false;
    }

    if (singleElement) {
        c_string_free (result, true);
        return c_strdup (singleElement);
    }
    else {
        if (lastTrailing) {
            c_string_append (result, lastTrailing);
        }
        return c_string_free (result, false);
    }
}

static void set_file_error (CError** error, const char* filename, const char* format_string, int saved_errno)
{
    char *msg = format_error_message (filename, format_string, saved_errno);

    c_set_error_literal (error, C_FILE_ERROR, c_file_error_from_errno (saved_errno), msg);
    c_free (msg);
}

static bool get_contents_stdio (const char* filename, FILE* f, char** contents, csize* length, CError** error)
{
    char buf[4096];
    csize bytes;  /* always <= sizeof(buf) */
    char *str = NULL;
    csize total_bytes = 0;
    csize total_allocated = 0;
    char *tmp;
    char *display_filename;

    c_assert (f != NULL);

    while (!feof (f)) {
        cint save_errno;

        bytes = fread (buf, 1, sizeof (buf), f);
        save_errno = errno;

        if (total_bytes > C_MAX_SIZE - bytes) {
            goto file_too_large;
        }

        /* Possibility of overflow eliminated above. */
        while (total_bytes + bytes >= total_allocated) {
            if (str) {
                if (total_allocated > C_MAX_SIZE / 2) {
                    goto file_too_large;
                }
                total_allocated *= 2;
            }
            else {
                total_allocated = C_MIN (bytes + 1, sizeof (buf));
            }

            tmp = c_realloc (str, total_allocated);

            if (tmp == NULL) {
                display_filename = c_filename_display_name (filename);
                c_set_error (error, C_FILE_ERROR, C_FILE_ERROR_NOMEM,
                                c_dngettext (PACKAGE_NAME,
                                                "Could not allocate %lu byte to read file “%s”",
                                                "Could not allocate %lu bytes to read file “%s”",
                                                (culong)total_allocated),
                                (culong) total_allocated, display_filename);
                c_free (display_filename);

                goto error;
            }
            str = tmp;
        }

        if (ferror (f)) {
            display_filename = c_filename_display_name (filename);
            c_set_error (error, C_FILE_ERROR,
                            c_file_error_from_errno (save_errno),
                            _("Error reading file “%s”: %s"),
                            display_filename,
                            c_strerror (save_errno));
            c_free (display_filename);

            goto error;
        }

        c_assert (str != NULL);
        memcpy (str + total_bytes, buf, bytes);

        total_bytes += bytes;
    }

    fclose (f);

    if (total_allocated == 0) {
        str = c_malloc0(sizeof(cchar));
        total_bytes = 0;
    }

    str[total_bytes] = '\0';

    if (length)
        *length = total_bytes;

    *contents = str;

    return true;

file_too_large:
    display_filename = c_filename_display_name (filename);
    c_set_error (error,C_FILE_ERROR, C_FILE_ERROR_FAILED, _("File “%s” is too large"), display_filename);
    c_free (display_filename);

error:

    c_free (str);
    fclose (f);

    return false;
}

static bool get_contents_regfile (const char* filename, struct stat* stat_buf, cint fd, char** contents, csize* length, CError** error)
{
    char *buf;
    csize bytes_read;
    csize size;
    csize alloc_size;
    char *display_filename;

    size = stat_buf->st_size;

    alloc_size = size + 1;
    buf = c_malloc0 (alloc_size);

    if (buf == NULL) {
        display_filename = c_filename_display_name (filename);
        c_set_error (error, C_FILE_ERROR, C_FILE_ERROR_NOMEM,
                        c_dngettext (PACKAGE_NAME, "Could not allocate %lu byte to read file “%s”",
                                        "Could not allocate %lu bytes to read file “%s”", (culong)alloc_size),
                        (culong) alloc_size,
                        display_filename);
        c_free (display_filename);
        goto error;
    }

    bytes_read = 0;
    while (bytes_read < size) {
        cssize rc;

        rc = read (fd, buf + bytes_read, size - bytes_read);
        if (rc < 0) {
            if (errno != EINTR) {
                int save_errno = errno;
                c_free (buf);
                display_filename = c_filename_display_name (filename);
                c_set_error (error, C_FILE_ERROR,
                                c_file_error_from_errno (save_errno),
                                _("Failed to read from file “%s”: %s"),
                                display_filename,
                                c_strerror (save_errno));
                c_free (display_filename);
                goto error;
            }
        }
        else if (rc == 0) {
            break;
        }
        else {
            bytes_read += rc;
        }
    }

    buf[bytes_read] = '\0';

    if (length) {
        *length = bytes_read;
    }

    *contents = buf;

    close (fd);

    return true;

error:

    close (fd);

    return false;
}

static bool get_contents_posix (const char* filename, char** contents, csize* length, CError** error)
{
    struct stat stat_buf;
    cint fd;

    /* O_BINARY useful on Cygwin */
    fd = open (filename, O_RDONLY);
    if (fd < 0) {
        int saved_errno = errno;
        if (error) {
            set_file_error (error, filename, _("Failed to open file “%s”: %s"), saved_errno);
        }

        return false;
    }

    /* I don't think this will ever fail, aside from ENOMEM, but. */
    if (fstat (fd, &stat_buf) < 0) {
        int saved_errno = errno;
        if (error)
            set_file_error (error,
                            filename,
                            _("Failed to get attributes of file “%s”: fstat() failed: %s"),
                            saved_errno);
        close (fd);

        return false;
    }

    if (stat_buf.st_size > 0 && S_ISREG (stat_buf.st_mode)) {
        bool retval = get_contents_regfile (filename, &stat_buf, fd, contents, length, error);
        return retval;
    }
    else {
        FILE *f;
        bool retval;

        f = fdopen (fd, "r");
        if (f == NULL) {
            int saved_errno = errno;
            if (error) {
                set_file_error (error, filename, _("Failed to open file “%s”: fdopen() failed: %s"), saved_errno);
            }
            return false;
        }

        retval = get_contents_stdio (filename, f, contents, length, error);

        return retval;
    }
}

static bool fd_should_be_fsynced (int fd, const char* test_file, CFileSetContentsFlags flags)
{
#ifdef HAVE_FSYNC
    struct stat statbuf;

    /* If the final destination exists and is > 0 bytes, we want to sync the
     * newly written file to ensure the data is on disk when we rename over
     * the destination. Otherwise if we get a system crash we can lose both
     * the new and the old file on some filesystems. (I.E. those that don't
     * guarantee the data is written to the disk before the metadata.)
     *
     * There is no difference (in file system terms) if the old file doesn’t
     * already exist, apart from the fact that if the system crashes and the new
     * data hasn’t been fsync()ed, there is only one bit of old data to lose (that
     * the file didn’t exist in the first place). In some situations, such as
     * trashing files, the old file never exists, so it seems reasonable to avoid
     * the fsync(). This is not a widely applicable optimisation though.
     */
    if ((flags & (C_FILE_SET_CONTENTS_CONSISTENT | C_FILE_SET_CONTENTS_DURABLE)) && (flags & C_FILE_SET_CONTENTS_ONLY_EXISTING)) {
        errno = 0;
        if (c_lstat (test_file, &statbuf) == 0)
            return (statbuf.st_size > 0);
        else if (errno == ENOENT)
            return FALSE;
        else
            return TRUE;  /* lstat() failed; be cautious */
    }
    else {
        return (flags & (C_FILE_SET_CONTENTS_CONSISTENT | C_FILE_SET_CONTENTS_DURABLE));
    }
#else  /* if !HAVE_FSYNC */
    return false;
#endif  /* !HAVE_FSYNC */
}

static bool write_to_file (const char* contents, csize length, int fd, const char* dest_file, bool do_fsync, CError** err)
{
#ifdef HAVE_FALLOCATE
    if (length > 0) {
        /* We do this on a 'best effort' basis... It may not be supported
         * on the underlying filesystem.
         */
        (void) fallocate (fd, 0, 0, length);
    }
#endif
    while (length > 0) {
        cssize s = write (fd, contents, C_MIN (length, C_MAX_SIZE));
        if (s < 0) {
            int saved_errno = errno;
            if (saved_errno == EINTR) {
                continue;
            }

            if (err) {
                set_file_error (err, dest_file, _("Failed to write file “%s”: write() failed: %s"), saved_errno);
            }
            close (fd);

            return false;
        }

        c_assert ((csize) s <= length);

        contents += s;
        length -= s;
    }


#ifdef HAVE_FSYNC
    errno = 0;
    if (do_fsync && c_fsync (fd) != 0) {
        int saved_errno = errno;
        if (err) {
            set_file_error (err, dest_file, _("Failed to write file “%s”: fsync() failed: %s"), saved_errno);
        }
        close (fd);

        return false;
    }
#endif

    errno = 0;
    if (!c_close (fd, err)) {
        return false;
    }

    return true;
}

static int get_tmp_file (char* tmpl, CTmpFileCallback f, int flags, int mode)
{
    char *XXXXXX;
    int count, fd;
    static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const int NLETTERS = sizeof (letters) - 1;
    clong value;
    cint64 now_us;
    static int counter = 0;

    c_return_val_if_fail (tmpl != NULL, -1);

    /* find the last occurrence of "XXXXXX" */
    XXXXXX = c_strrstr (tmpl, "XXXXXX");

    if (!XXXXXX || strncmp (XXXXXX, "XXXXXX", 6)) {
        errno = EINVAL;
        return -1;
    }

    /* Get some more or less random data.  */
    now_us = c_get_real_time ();
    value = ((now_us % C_USEC_PER_SEC) ^ (now_us / C_USEC_PER_SEC)) + counter++;

    for (count = 0; count < 100; value += 7777, ++count) {
        clong v = value;

        /* Fill in the random bits.  */
        XXXXXX[0] = letters[v % NLETTERS];
        v /= NLETTERS;
        XXXXXX[1] = letters[v % NLETTERS];
        v /= NLETTERS;
        XXXXXX[2] = letters[v % NLETTERS];
        v /= NLETTERS;
        XXXXXX[3] = letters[v % NLETTERS];
        v /= NLETTERS;
        XXXXXX[4] = letters[v % NLETTERS];
        v /= NLETTERS;
        XXXXXX[5] = letters[v % NLETTERS];

        fd = f (tmpl, flags, mode);

        if (fd >= 0) {
            return fd;
        }
        else if (errno != EEXIST) {
            return -1;
        }
    }

    errno = EEXIST;
    return -1;
}


static cint wrap_g_mkdir (const char* filename, int flags C_UNUSED, int mode)
{
    return c_mkdir (filename, mode);
}

static cint wrap_g_open (const char* filename, int flags, int mode)
{
    return c_open (filename, flags, mode);
}

static int g_get_tmp_name (const char* tmpl, char** name_used, CTmpFileCallback f, cint flags, cint mode, CError** error)
{
    int retval;
    const char *tmpdir;
    const char *sep;
    char *fulltemplate;
    const char *slash;

    if (tmpl == NULL) {
        tmpl = ".XXXXXX";
    }

    if ((slash = strchr (tmpl, C_DIR_SEPARATOR)) != NULL
#ifdef G_OS_WIN32
        || (strchr (tmpl, '/') != NULL && (slash = "/"))
#endif
        )
    {
        char *display_tmpl = c_filename_display_name (tmpl);
        char c[2];
        c[0] = *slash;
        c[1] = '\0';

        c_set_error (error, C_FILE_ERROR, C_FILE_ERROR_FAILED, _("Template “%s” invalid, should not contain a “%s”"), display_tmpl, c);
        c_free (display_tmpl);

        return -1;
    }

    if (strstr (tmpl, "XXXXXX") == NULL) {
        char *display_tmpl = c_filename_display_name (tmpl);
        c_set_error (error, C_FILE_ERROR, C_FILE_ERROR_FAILED, _("Template “%s” doesn’t contain XXXXXX"), display_tmpl);
        c_free (display_tmpl);
        return -1;
    }

    tmpdir = c_get_tmp_dir ();

    if (C_IS_DIR_SEPARATOR (tmpdir [strlen (tmpdir) - 1])) {
        sep = "";
    }
    else {
        sep = C_DIR_SEPARATOR_S;
    }

    fulltemplate = c_strconcat (tmpdir, sep, tmpl, NULL);

    retval = get_tmp_file (fulltemplate, f, flags, mode);
    if (retval == -1) {
        int saved_errno = errno;
        if (error)
            set_file_error (error,
                            fulltemplate,
                            _("Failed to create file “%s”: %s"),
                            saved_errno);
        c_free (fulltemplate);
        return -1;
    }

    *name_used = fulltemplate;

    return retval;
}

static char* g_build_path_va (const char* separator, const char* first_element, va_list* args, char** str_array)
{
    CString *result;
    int separator_len = strlen (separator);
    bool is_first = true;
    bool have_leading = false;
    const char* single_element = NULL;
    const char* next_element;
    const char* last_trailing = NULL;
    int i = 0;

    result = c_string_new (NULL);

    if (str_array) {
        next_element = str_array[i++];
    }
    else {
        next_element = first_element;
    }

    while (true) {
        const char *element;
        const char *start;
        const char *end;

        if (next_element) {
            element = next_element;
            if (str_array) {
                next_element = str_array[i++];
            }
            else {
                next_element = va_arg (*args, char *);
            }
        }
        else {
            break;
        }

        /* Ignore empty elements */
        if (!*element) {
            continue;
        }

        start = element;

        if (separator_len) {
            while (strncmp (start, separator, separator_len) == 0) {
                start += separator_len;
            }
        }

        end = start + strlen (start);

        if (separator_len) {
            while (end >= start + separator_len && strncmp (end - separator_len, separator, separator_len) == 0) {
                end -= separator_len;
            }

            last_trailing = end;
            while (last_trailing >= element + separator_len && strncmp (last_trailing - separator_len, separator, separator_len) == 0) {
                last_trailing -= separator_len;
            }

            if (!have_leading) {
                if (last_trailing <= start) {
                    single_element = element;
                }

                c_string_append_len (result, element, start - element);
                have_leading = true;
            }
            else {
                single_element = NULL;
            }
        }

        if (end == start) {
            continue;
        }

        if (!is_first) {
            c_string_append (result, separator);
        }

        c_string_append_len (result, start, end - start);
        is_first = false;
    }

    if (single_element) {
        c_string_free (result, true);
        return c_strdup (single_element);
    }
    else {
        if (last_trailing) {
            c_string_append (result, last_trailing);
        }

        return c_string_free (result, false);
    }
}

static char* g_build_filename_va (const char* first_argument, va_list* args, char** str_array)
{
    char *str;

#ifndef G_OS_WIN32
    str = g_build_path_va (C_DIR_SEPARATOR_S, first_argument, args, str_array);
#else
    str = c_build_pathname_va (first_argument, args, str_array);
#endif

    return str;
}

static void arr_move_left1(cuint64 startPos, char* buf)
{
    c_return_if_fail(buf);

    cuint64 len = c_strlen(buf);

    // int idx = startPos + 1;
    for (; startPos < len - 1; startPos++) {
        // printf("%d -- %d -- %d -- %c\n", startPos, startPos + 1, len, buf[startPos + 1]);
        buf[startPos] = buf[startPos + 1];
    }
    buf[len - 1] = '\0';
}
