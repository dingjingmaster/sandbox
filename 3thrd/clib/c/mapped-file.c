
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

#include "mapped-file.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "str.h"
#include "error.h"
#include "bytes.h"
#include "atomic.h"
#include "convert.h"
#include "file-utils.h"

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif

struct _CMappedFile
{
    char*   contents;
    csize   length;
    void*   freeFunc;
    int     refCount;
};


static void c_mapped_file_destroy (CMappedFile* file);
static CMappedFile* mapped_file_new_from_fd (int fd, bool writable, const char* filename, CError** error);


CMappedFile* c_mapped_file_new (const char* filename, bool writable, CError** error)
{
    int fd;
    CMappedFile* file;

    c_return_val_if_fail (filename != NULL, NULL);
    c_return_val_if_fail (!error || *error == NULL, NULL);

    fd = c_open (filename, (writable ? O_RDWR : O_RDONLY) | _O_BINARY, 0);
    if (fd == -1) {
        int save_errno = errno;
        char* displayFilename = c_filename_display_name (filename);

        c_set_error (error, C_FILE_ERROR,
                        c_file_error_from_errno (save_errno),
                        _("Failed to open file “%s”: open() failed: %s"),
                        displayFilename,
                        c_strerror (save_errno));
        c_free (displayFilename);
        return NULL;
    }

    file = mapped_file_new_from_fd (fd, writable, filename, error);

    close (fd);

    return file;
}

CMappedFile* c_mapped_file_new_from_fd (cint fd, bool writable, CError** error)
{
    return mapped_file_new_from_fd (fd, writable, NULL, error);
}

csize c_mapped_file_get_length (CMappedFile* file)
{
    c_return_val_if_fail (file != NULL, 0);

    return file->length;
}

char* c_mapped_file_get_contents (CMappedFile* file)
{
    c_return_val_if_fail (file != NULL, NULL);

    return file->contents;
}

CBytes* c_mapped_file_get_bytes (CMappedFile* file)
{
    c_return_val_if_fail (file != NULL, NULL);

    return c_bytes_new_with_free_func (file->contents, file->length, (CDestroyNotify) c_mapped_file_unref, c_mapped_file_ref (file));
}

CMappedFile* c_mapped_file_ref (CMappedFile* file)
{
    c_return_val_if_fail (file != NULL, NULL);

    c_atomic_int_inc (&file->refCount);

    return file;
}

void c_mapped_file_unref (CMappedFile* file)
{
    c_return_if_fail (file != NULL);

    if (c_atomic_int_dec_and_test (&file->refCount)) {
        c_mapped_file_destroy (file);
    }
}

void c_mapped_file_free (CMappedFile* file)
{
    c_mapped_file_unref (file);
}



static void c_mapped_file_destroy (CMappedFile* file)
{
    c_return_if_fail(file);

    if (file->length) {
        munmap (file->contents, file->length);
    }

    c_free(file);
}


static CMappedFile* mapped_file_new_from_fd (int fd, bool writable, const char* filename, CError** error)
{
    CMappedFile *file;
    struct stat st;

    file = c_malloc0(sizeof (CMappedFile));
    file->refCount = 1;
    file->freeFunc = c_mapped_file_destroy;

    if (fstat (fd, &st) == -1) {
        int saveErrno = errno;
        char* displayFilename = filename ? c_filename_display_name (filename) : NULL;

        c_set_error (error, C_FILE_ERROR,
                        c_file_error_from_errno (saveErrno),
                        _("Failed to get attributes of file “%s%s%s%s”: fstat() failed: %s"),
                        displayFilename ? displayFilename : "fd",
                        displayFilename ? "' " : "",
                        displayFilename ? displayFilename : "",
                        displayFilename ? "'" : "", c_strerror (saveErrno));
        c_free (displayFilename);
        goto out;
    }

    if (st.st_size == 0 && S_ISREG (st.st_mode)) {
        file->length = 0;
        file->contents = NULL;
        return file;
    }

    file->contents = MAP_FAILED;

    if (sizeof (st.st_size) > sizeof (csize) && st.st_size > (off_t) C_MAX_SIZE) {
        errno = EINVAL;
    }
    else {
        file->length = (csize) st.st_size;
        file->contents = (char*) mmap (NULL, file->length, writable ? PROT_READ|PROT_WRITE : PROT_READ, MAP_PRIVATE, fd, 0);
    }

    if (file->contents == MAP_FAILED) {
        int save_errno = errno;
        char* displayFilename = filename ? c_filename_display_name (filename) : NULL;

        c_set_error (error, C_FILE_ERROR,
                        c_file_error_from_errno (save_errno),
                        _("Failed to map %s%s%s%s: mmap() failed: %s"),
                        displayFilename ? displayFilename : "fd",
                        displayFilename ? "' " : "",
                        displayFilename ? displayFilename : "",
                        displayFilename ? "'" : "",
                        c_strerror (save_errno));
        c_free (displayFilename);
        goto out;
    }

    return file;

out:
    c_free(file);

    return NULL;
}

