/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc_file.c - File-based credential cache */
/*
 * Copyright 1990,1991,1992,1993,1994,2000,2004,2007 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Original stdio support copyright 1995 by Cygnus Support.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * A psuedo-BNF grammar for the FILE credential cache format is:
 *
 * file ::=
 *   version (2 bytes; 05 01 for version 1 through 05 04 for version 4)
 *   header [not present before version 4]
 *   principal
 *   credential1
 *   credential2
 *   ...
 *
 * header ::=
 *   headerlen (16 bits)
 *   header1tag (16 bits)
 *   header1len (16 bits)
 *   header1val (header1len bytes)
 *
 * See ccmarshal.c for the principal and credential formats.  Although versions
 * 1 and 2 of the FILE format use native byte order for integer representations
 * within principals and credentials, the integer fields in the grammar above
 * are always in big-endian byte order.
 *
 * Only one header tag is currently defined.  The tag value is 1
 * (FCC_TAG_DELTATIME), and its contents are two 32-bit integers giving the
 * seconds and microseconds of the time offset of the KDC relative to the
 * client.
 *
 * Each of the file ccache functions opens and closes the file whenever it
 * needs to access it.
 *
 * This module depends on UNIX-like file descriptors, and UNIX-like behavior
 * from the functions: open, close, read, write, lseek.
 */

#include "k5-int.h"
#include "cc-int.h"

#include <stdio.h>
#include <errno.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

extern const krb5_cc_ops krb5_cc_file_ops;

krb5_error_code krb5_change_cache(void);

static krb5_error_code interpret_errno(krb5_context, int);

/* The cache format version is a positive integer, represented in the cache
 * file as a two-byte big endian number with 0x0500 added to it. */
#define FVNO_BASE 0x0500

#define FCC_TAG_DELTATIME       1

#ifndef TKT_ROOT
#ifdef MSDOS_FILESYSTEM
#define TKT_ROOT "\\tkt"
#else
#define TKT_ROOT "/tmp/tkt"
#endif
#endif

typedef struct fcc_data_st {
    k5_cc_mutex lock;
    char *filename;
} fcc_data;

/* Iterator over file caches.  */
struct krb5_fcc_ptcursor_data {
    krb5_boolean first;
};

/* Iterator over a cache. */
typedef struct _krb5_fcc_cursor {
    FILE *fp;
    int version;
} krb5_fcc_cursor;

k5_cc_mutex krb5int_cc_file_mutex = K5_CC_MUTEX_PARTIAL_INITIALIZER;

/* Add fname to the standard error message for ret. */
static krb5_error_code
set_errmsg_filename(krb5_context context, krb5_error_code ret,
                    const char *fname)
{
    if (!ret)
        return 0;
    k5_setmsg(context, ret, "%s (filename: %s)", error_message(ret), fname);
    return ret;
}

/* Get the size of the cache file as a size_t, or SIZE_MAX if it is too
 * large to be represented as a size_t. */
static krb5_error_code
get_size(krb5_context context, FILE *fp, size_t *size_out)
{
    struct stat sb;

    *size_out = 0;
    if (fstat(fileno(fp), &sb) == -1)
        return interpret_errno(context, errno);
    if (sizeof(off_t) > sizeof(size_t) && sb.st_size > (off_t)SIZE_MAX)
        *size_out = SIZE_MAX;
    else
        *size_out = sb.st_size;
    return 0;
}

/* Read len bytes from fp, storing them in buf.  Return KRB5_CC_END
 * if not enough bytes are present. */
static krb5_error_code
read_bytes(krb5_context context, FILE *fp, void *buf, size_t len)
{
    size_t nread;

    nread = fread(buf, 1, len, fp);
    if (nread < len)
        return ferror(fp) ? errno : KRB5_CC_END;
    return 0;
}

/* Load four bytes from the cache file.  Add them to buf (if set) and return
 * their value as a 32-bit unsigned integer according to the file format. */
static krb5_error_code
read32(krb5_context context, FILE *fp, int version, struct k5buf *buf,
       uint32_t *out)
{
    krb5_error_code ret;
    char bytes[4];

    ret = read_bytes(context, fp, bytes, 4);
    if (ret)
        return ret;
    if (buf != NULL)
        k5_buf_add_len(buf, bytes, 4);
    *out = (version < 3) ? load_32_n(bytes) : load_32_be(bytes);
    return 0;
}

/* Load two bytes from the cache file and return their value as a 16-bit
 * unsigned integer according to the file format. */
static krb5_error_code
read16(krb5_context context, FILE *fp, int version, uint16_t *out)
{
    krb5_error_code ret;
    char bytes[2];

    ret = read_bytes(context, fp, bytes, 2);
    if (ret)
        return ret;
    *out = (version < 3) ? load_16_n(bytes) : load_16_be(bytes);
    return 0;
}

/* Read len bytes from the cache file and add them to buf. */
static krb5_error_code
load_bytes(krb5_context context, FILE *fp, size_t len, struct k5buf *buf)
{
    void *ptr;

    ptr = k5_buf_get_space(buf, len);
    return (ptr == NULL) ? KRB5_CC_NOMEM : read_bytes(context, fp, ptr, len);
}

/* Load a 32-bit length and data from the cache file into buf, but not more
 * than maxsize bytes. */
static krb5_error_code
load_data(krb5_context context, FILE *fp, int version, size_t maxsize,
          struct k5buf *buf)
{
    krb5_error_code ret;
    uint32_t count;

    ret = read32(context, fp, version, buf, &count);
    if (ret)
        return ret;
    if (count > maxsize)
        return KRB5_CC_FORMAT;
    return load_bytes(context, fp, count, buf);
}

/* Load a marshalled principal from the cache file into buf, without
 * unmarshalling it. */
static krb5_error_code
load_principal(krb5_context context, FILE *fp, int version, size_t maxsize,
               struct k5buf *buf)
{
    krb5_error_code ret;
    uint32_t count;

    if (version > 1) {
        ret = load_bytes(context, fp, 4, buf);
        if (ret)
            return ret;
    }
    ret = read32(context, fp, version, buf, &count);
    if (ret)
        return ret;
    /* Add one for the realm (except in version 1 which already counts it). */
    if (version != 1)
        count++;
    while (count-- > 0) {
        ret = load_data(context, fp, version, maxsize, buf);
        if (ret)
            return ret;
    }
    return 0;
}

/* Load a marshalled credential from the cache file into buf, without
 * unmarshalling it. */
static krb5_error_code
load_cred(krb5_context context, FILE *fp, int version, size_t maxsize,
          struct k5buf *buf)
{
    krb5_error_code ret;
    uint32_t count, i;

    /* client and server */
    ret = load_principal(context, fp, version, maxsize, buf);
    if (ret)
        return ret;
    ret = load_principal(context, fp, version, maxsize, buf);
    if (ret)
        return ret;

    /* keyblock (enctype, enctype again for version 3, length, value) */
    ret = load_bytes(context, fp, (version == 3) ? 4 : 2, buf);
    if (ret)
        return ret;
    ret = load_data(context, fp, version, maxsize, buf);
    if (ret)
        return ret;

    /* times (4*4 bytes), is_skey (1 byte), ticket flags (4 bytes) */
    ret = load_bytes(context, fp, 4 * 4 + 1 + 4, buf);
    if (ret)
        return ret;

    /* addresses and authdata, both lists of {type, length, data} */
    for (i = 0; i < 2; i++) {
        ret = read32(context, fp, version, buf, &count);
        if (ret)
            return ret;
        while (count-- > 0) {
            ret = load_bytes(context, fp, 2, buf);
            if (ret)
                return ret;
            ret = load_data(context, fp, version, maxsize, buf);
            if (ret)
                return ret;
        }
    }

    /* ticket and second_ticket */
    ret = load_data(context, fp, version, maxsize, buf);
    if (ret)
        return ret;
    return load_data(context, fp, version, maxsize, buf);
}

static krb5_error_code
read_principal(krb5_context context, FILE *fp, int version,
               krb5_principal *princ)
{
    krb5_error_code ret;
    struct k5buf buf;
    size_t maxsize;

    *princ = NULL;
    k5_buf_init_dynamic(&buf);

    /* Read the principal representation into memory. */
    ret = get_size(context, fp, &maxsize);
    if (ret)
        goto cleanup;
    ret = load_principal(context, fp, version, maxsize, &buf);
    if (ret)
        goto cleanup;
    ret = k5_buf_status(&buf);
    if (ret)
        goto cleanup;

    /* Unmarshal it from buf into princ. */
    ret = k5_unmarshal_princ(buf.data, buf.len, version, princ);

cleanup:
    k5_buf_free(&buf);
    return ret;
}

/*
 * Open and lock an existing cache file.  If writable is true, open it for
 * writing (with O_APPEND) and get an exclusive lock; otherwise open it for
 * reading and get a shared lock.
 */
static krb5_error_code
open_cache_file(krb5_context context, const char *filename,
                krb5_boolean writable, FILE **fp_out)
{
    krb5_error_code ret;
    int fd, flags, lockmode;
    FILE *fp;

    *fp_out = NULL;

    flags = writable ? (O_RDWR | O_APPEND) : O_RDONLY;
    fd = open(filename, flags | O_BINARY | O_CLOEXEC, 0600);
    if (fd == -1)
        return interpret_errno(context, errno);
    set_cloexec_fd(fd);

    lockmode = writable ? KRB5_LOCKMODE_EXCLUSIVE : KRB5_LOCKMODE_SHARED;
    ret = krb5_lock_file(context, fd, lockmode);
    if (ret) {
        (void)close(fd);
        return ret;
    }

    fp = fdopen(fd, writable ? "r+b" : "rb");
    if (fp == NULL) {
        (void)krb5_unlock_file(context, fd);
        (void)close(fd);
        return KRB5_CC_NOMEM;
    }

    *fp_out = fp;
    return 0;
}

/* Unlock and close the cache file.  Do nothing if fp is NULL. */
static krb5_error_code
close_cache_file(krb5_context context, FILE *fp)
{
    int st;
    krb5_error_code ret;

    if (fp == NULL)
        return 0;
    ret = krb5_unlock_file(context, fileno(fp));
    st = fclose(fp);
    if (ret)
        return ret;
    return st ? interpret_errno(context, errno) : 0;
}

/* Read the cache file header.  Set time offsets in context from the header if
 * appropriate.  Set *version_out to the cache file format version. */
static krb5_error_code
read_header(krb5_context context, FILE *fp, int *version_out)
{
    krb5_error_code ret;
    krb5_os_context os_ctx = &context->os_context;
    uint16_t fields_len, tag, flen;
    uint32_t time_offset, usec_offset;
    char i16buf[2];
    int version;

    *version_out = 0;

    /* Get the file format version. */
    ret = read_bytes(context, fp, i16buf, 2);
    if (ret)
        return KRB5_CC_FORMAT;
    version = load_16_be(i16buf) - FVNO_BASE;
    if (version < 1 || version > 4)
        return KRB5_CCACHE_BADVNO;
    *version_out = version;

    /* Tagged header fields begin with version 4. */
    if (version < 4)
        return 0;

    if (read16(context, fp, version, &fields_len))
        return KRB5_CC_FORMAT;
    while (fields_len) {
        if (fields_len < 4 || read16(context, fp, version, &tag) ||
            read16(context, fp, version, &flen) || flen > fields_len - 4)
            return KRB5_CC_FORMAT;

        switch (tag) {
        case FCC_TAG_DELTATIME:
            if (flen != 8 ||
                read32(context, fp, version, NULL, &time_offset) ||
                read32(context, fp, version, NULL, &usec_offset))
                return KRB5_CC_FORMAT;

            if (!(context->library_options & KRB5_LIBOPT_SYNC_KDCTIME) ||
                (os_ctx->os_flags & KRB5_OS_TOFFSET_VALID))
                break;

            os_ctx->time_offset = time_offset;
            os_ctx->usec_offset = usec_offset;
            os_ctx->os_flags = ((os_ctx->os_flags & ~KRB5_OS_TOFFSET_TIME) |
                                KRB5_OS_TOFFSET_VALID);
            break;

        default:
            if (flen && fseek(fp, flen, SEEK_CUR) != 0)
                return KRB5_CC_FORMAT;
            break;
        }
        fields_len -= (4 + flen);
    }
    return 0;
}

/* Create or overwrite the cache file with a header and default principal. */
static krb5_error_code KRB5_CALLCONV
fcc_initialize(krb5_context context, krb5_ccache id, krb5_principal princ)
{
    krb5_error_code ret;
    krb5_os_context os_ctx = &context->os_context;
    fcc_data *data = id->data;
    char i16buf[2], i32buf[4];
    uint16_t fields_len;
    ssize_t nwritten;
    int st, flags, version, fd = -1;
    struct k5buf buf = EMPTY_K5BUF;
    krb5_boolean file_locked = FALSE;

    k5_cc_mutex_lock(context, &data->lock);

    unlink(data->filename);
    flags = O_CREAT | O_EXCL | O_RDWR | O_BINARY | O_CLOEXEC;
    fd = open(data->filename, flags, 0600);
    if (fd == -1) {
        ret = interpret_errno(context, errno);
        goto cleanup;
    }
    set_cloexec_fd(fd);

#if defined(HAVE_FCHMOD) || defined(HAVE_CHMOD)
#ifdef HAVE_FCHMOD
    st = fchmod(fd, S_IRUSR | S_IWUSR);
#else
    st = chmod(data->filename, S_IRUSR | S_IWUSR);
#endif
    if (st == -1) {
        ret = interpret_errno(context, errno);
        goto cleanup;
    }
#endif

    ret = krb5_lock_file(context, fd, KRB5_LOCKMODE_EXCLUSIVE);
    if (ret)
        goto cleanup;
    file_locked = TRUE;

    /* Prepare the header and principal in buf. */
    k5_buf_init_dynamic(&buf);
    version = context->fcc_default_format - FVNO_BASE;
    store_16_be(FVNO_BASE + version, i16buf);
    k5_buf_add_len(&buf, i16buf, 2);
    if (version >= 4) {
        /* Add tagged header fields. */
        fields_len = 0;
        if (os_ctx->os_flags & KRB5_OS_TOFFSET_VALID)
            fields_len += 12;
        store_16_be(fields_len, i16buf);
        k5_buf_add_len(&buf, i16buf, 2);
        if (os_ctx->os_flags & KRB5_OS_TOFFSET_VALID) {
            /* Add time offset tag. */
            store_16_be(FCC_TAG_DELTATIME, i16buf);
            k5_buf_add_len(&buf, i16buf, 2);
            store_16_be(8, i16buf);
            k5_buf_add_len(&buf, i16buf, 2);
            store_32_be(os_ctx->time_offset, i32buf);
            k5_buf_add_len(&buf, i32buf, 4);
            store_32_be(os_ctx->usec_offset, i32buf);
            k5_buf_add_len(&buf, i32buf, 4);
        }
    }
    k5_marshal_princ(&buf, version, princ);
    ret = k5_buf_status(&buf);
    if (ret)
        goto cleanup;

    /* Write the header and principal. */
    nwritten = write(fd, buf.data, buf.len);
    if (nwritten == -1)
        ret = interpret_errno(context, errno);
    if ((size_t)nwritten != buf.len)
        ret = KRB5_CC_IO;

cleanup:
    k5_buf_free(&buf);
    if (file_locked)
        krb5_unlock_file(context, fd);
    if (fd != -1)
        close(fd);
    k5_cc_mutex_unlock(context, &data->lock);
    krb5_change_cache();
    return set_errmsg_filename(context, ret, data->filename);
}

/* Release an fcc_data object. */
static void
free_fccdata(krb5_context context, fcc_data *data)
{
    k5_cc_mutex_assert_unlocked(context, &data->lock);
    free(data->filename);
    k5_cc_mutex_destroy(&data->lock);
    free(data);
}

/* Release the ccache handle. */
static krb5_error_code KRB5_CALLCONV
fcc_close(krb5_context context, krb5_ccache id)
{
    free_fccdata(context, id->data);
    free(id);
    return 0;
}

/* Destroy the cache file and release the handle. */
static krb5_error_code KRB5_CALLCONV
fcc_destroy(krb5_context context, krb5_ccache id)
{
    krb5_error_code ret = 0;
    fcc_data *data = id->data;
    int st, fd;
    struct stat buf;
    unsigned long i, size;
    unsigned int wlen;
    char zeros[BUFSIZ];

    k5_cc_mutex_lock(context, &data->lock);

    fd = open(data->filename, O_RDWR | O_BINARY | O_CLOEXEC, 0);
    if (fd < 0) {
        ret = interpret_errno(context, errno);
        goto cleanup;
    }
    set_cloexec_fd(fd);

#ifdef MSDOS_FILESYSTEM
    /*
     * "Disgusting bit of UNIX trivia" - that's how the writers of NFS describe
     * the ability of UNIX to still write to a file which has been unlinked.
     * Naturally, the PC can't do this.  As a result, we have to delete the
     * file after we wipe it clean, but that throws off all the error handling
     * code.  So we have do the work ourselves.
     */
    st = fstat(fd, &buf);
    if (st == -1) {
        ret = interpret_errno(context, errno);
        size = 0;               /* Nothing to wipe clean */
    } else {
        size = (unsigned long)buf.st_size;
    }

    memset(zeros, 0, BUFSIZ);
    while (size > 0) {
        wlen = (int)((size > BUFSIZ) ? BUFSIZ : size); /* How much to write */
        i = write(fd, zeros, wlen);
        if (i < 0) {
            ret = interpret_errno(context, errno);
            /* Don't jump to cleanup--we still want to delete the file. */
            break;
        }
        size -= i;
    }

    (void)close(fd);

    st = unlink(data->filename);
    if (st < 0) {
        ret = interpret_errno(context, errno);
        goto cleanup;
    }

#else /* MSDOS_FILESYSTEM */

    st = unlink(data->filename);
    if (st < 0) {
        ret = interpret_errno(context, errno);
        (void)close(fd);
        goto cleanup;
    }

    st = fstat(fd, &buf);
    if (st < 0) {
        ret = interpret_errno(context, errno);
        (void)close(fd);
        goto cleanup;
    }

    /* XXX This may not be legal XXX */
    size = (unsigned long)buf.st_size;
    memset(zeros, 0, BUFSIZ);
    for (i = 0; i < size / BUFSIZ; i++) {
        if (write(fd, zeros, BUFSIZ) < 0) {
            ret = interpret_errno(context, errno);
            (void)close(fd);
            goto cleanup;
        }
    }

    wlen = size % BUFSIZ;
    if (write(fd, zeros, wlen) < 0) {
        ret = interpret_errno(context, errno);
        (void)close(fd);
        goto cleanup;
    }

    st = close(fd);

    if (st)
        ret = interpret_errno(context, errno);

#endif /* MSDOS_FILESYSTEM */

cleanup:
    (void)set_errmsg_filename(context, ret, data->filename);
    k5_cc_mutex_unlock(context, &data->lock);
    free_fccdata(context, data);
    free(id);

    krb5_change_cache();
    return ret;
}

extern const krb5_cc_ops krb5_fcc_ops;

/* Create a file ccache handle for the pathname given by residual. */
static krb5_error_code KRB5_CALLCONV
fcc_resolve(krb5_context context, krb5_ccache *id, const char *residual)
{
    krb5_ccache lid;
    krb5_error_code ret;
    fcc_data *data;

    data = malloc(sizeof(fcc_data));
    if (data == NULL)
        return KRB5_CC_NOMEM;
    data->filename = strdup(residual);
    if (data->filename == NULL) {
        free(data);
        return KRB5_CC_NOMEM;
    }
    ret = k5_cc_mutex_init(&data->lock);
    if (ret) {
        free(data->filename);
        free(data);
        return ret;
    }

    lid = malloc(sizeof(struct _krb5_ccache));
    if (lid == NULL) {
        free_fccdata(context, data);
        return KRB5_CC_NOMEM;
    }

    lid->ops = &krb5_fcc_ops;
    lid->data = data;
    lid->magic = KV5M_CCACHE;

    /* Other routines will get errors on open, and callers must expect them, if
     * cache is non-existent/unusable. */
    *id = lid;
    return 0;
}

/* Prepare for a sequential iteration over the cache file. */
static krb5_error_code KRB5_CALLCONV
fcc_start_seq_get(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor)
{
    krb5_fcc_cursor *fcursor = NULL;
    krb5_error_code ret;
    krb5_principal princ = NULL;
    fcc_data *data = id->data;
    FILE *fp = NULL;
    int version;

    k5_cc_mutex_lock(context, &data->lock);

    fcursor = malloc(sizeof(krb5_fcc_cursor));
    if (fcursor == NULL) {
        ret = KRB5_CC_NOMEM;
        goto cleanup;
    }

    /* Open the cache file and read the header. */
    ret = open_cache_file(context, data->filename, FALSE, &fp);
    if (ret)
        goto cleanup;
    ret = read_header(context, fp, &version);
    if (ret)
        goto cleanup;

    /* Read past the default client principal name. */
    ret = read_principal(context, fp, version, &princ);
    if (ret)
        goto cleanup;

    /* Drop the shared file lock but retain the file handle. */
    (void)krb5_unlock_file(context, fileno(fp));
    fcursor->fp = fp;
    fp = NULL;
    fcursor->version = version;
    *cursor = (krb5_cc_cursor)fcursor;
    fcursor = NULL;

cleanup:
    (void)close_cache_file(context, fp);
    free(fcursor);
    krb5_free_principal(context, princ);
    k5_cc_mutex_unlock(context, &data->lock);
    return set_errmsg_filename(context, ret, data->filename);
}

/* Get the next credential from the cache file. */
static krb5_error_code KRB5_CALLCONV
fcc_next_cred(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor,
              krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_fcc_cursor *fcursor = *cursor;
    fcc_data *data = id->data;
    struct k5buf buf;
    size_t maxsize;
    krb5_boolean file_locked = FALSE;

    memset(creds, 0, sizeof(*creds));
    k5_cc_mutex_lock(context, &data->lock);
    k5_buf_init_dynamic(&buf);

    ret = krb5_lock_file(context, fileno(fcursor->fp), KRB5_LOCKMODE_SHARED);
    if (ret)
        goto cleanup;
    file_locked = TRUE;

    /* Load a marshalled cred into memory. */
    ret = get_size(context, fcursor->fp, &maxsize);
    if (ret)
        goto cleanup;
    ret = load_cred(context, fcursor->fp, fcursor->version, maxsize, &buf);
    if (ret)
        goto cleanup;
    ret = k5_buf_status(&buf);
    if (ret)
        goto cleanup;

    /* Unmarshal it from buf into creds. */
    ret = k5_unmarshal_cred(buf.data, buf.len, fcursor->version, creds);

cleanup:
    if (file_locked)
        (void)krb5_unlock_file(context, fileno(fcursor->fp));
    k5_cc_mutex_unlock(context, &data->lock);
    k5_buf_free(&buf);
    return set_errmsg_filename(context, ret, data->filename);
}

/* Release an iteration cursor. */
static krb5_error_code KRB5_CALLCONV
fcc_end_seq_get(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor)
{
    krb5_fcc_cursor *fcursor = *cursor;

    (void)fclose(fcursor->fp);
    free(fcursor);
    *cursor = NULL;
    return 0;
}

/* Generate a unique file ccache using the given template (which will be
 * modified to contain the actual name of the file). */
krb5_error_code
krb5int_fcc_new_unique(krb5_context context, char *template, krb5_ccache *id)
{
    krb5_ccache lid;
    int fd;
    krb5_error_code ret;
    fcc_data *data;
    char fcc_fvno[2];
    int16_t fcc_flen = 0;
    int errsave, cnt;

    fd = mkstemp(template);
    if (fd == -1)
        return interpret_errno(context, errno);
    set_cloexec_fd(fd);

    /* Allocate memory */
    data = malloc(sizeof(fcc_data));
    if (data == NULL) {
        close(fd);
        unlink(template);
        return KRB5_CC_NOMEM;
    }

    data->filename = strdup(template);
    if (data->filename == NULL) {
        free(data);
        close(fd);
        unlink(template);
        return KRB5_CC_NOMEM;
    }

    ret = k5_cc_mutex_init(&data->lock);
    if (ret) {
        free(data->filename);
        free(data);
        close(fd);
        unlink(template);
        return ret;
    }
    k5_cc_mutex_lock(context, &data->lock);

    /* Ignore user's umask, set mode = 0600 */
#ifndef HAVE_FCHMOD
#ifdef HAVE_CHMOD
    chmod(data->filename, S_IRUSR | S_IWUSR);
#endif
#else
    fchmod(fd, S_IRUSR | S_IWUSR);
#endif
    store_16_be(context->fcc_default_format, fcc_fvno);
    cnt = write(fd, &fcc_fvno, 2);
    if (cnt != 2) {
        errsave = errno;
        (void)close(fd);
        (void)unlink(data->filename);
        ret = (cnt == -1) ? interpret_errno(context, errsave) : KRB5_CC_IO;
        goto err_out;
    }
    /* For version 4 we save a length for the rest of the header */
    if (context->fcc_default_format == FVNO_BASE + 4) {
        cnt = write(fd, &fcc_flen, sizeof(fcc_flen));
        if (cnt != sizeof(fcc_flen)) {
            errsave = errno;
            (void)close(fd);
            (void)unlink(data->filename);
            ret = (cnt == -1) ? interpret_errno(context, errsave) : KRB5_CC_IO;
            goto err_out;
        }
    }
    if (close(fd) == -1) {
        errsave = errno;
        (void)unlink(data->filename);
        ret = interpret_errno(context, errsave);
        goto err_out;
    }

    k5_cc_mutex_assert_locked(context, &data->lock);
    k5_cc_mutex_unlock(context, &data->lock);
    lid = malloc(sizeof(*lid));
    if (lid == NULL) {
        free_fccdata(context, data);
        return KRB5_CC_NOMEM;
    }

    lid->ops = &krb5_fcc_ops;
    lid->data = data;
    lid->magic = KV5M_CCACHE;

    *id = lid;

    krb5_change_cache();
    return 0;

err_out:
    (void)set_errmsg_filename(context, ret, data->filename);
    k5_cc_mutex_unlock(context, &data->lock);
    k5_cc_mutex_destroy(&data->lock);
    free(data->filename);
    free(data);
    return ret;
}

/*
 * Create a new file cred cache whose name is guaranteed to be unique.  The
 * name begins with the string TKT_ROOT (from fcc.h).  The cache file is not
 * opened, but the new filename is reserved.
 */
static krb5_error_code KRB5_CALLCONV
fcc_generate_new(krb5_context context, krb5_ccache *id)
{
    char scratch[sizeof(TKT_ROOT) + 7]; /* Room for XXXXXX and terminator */

    (void)snprintf(scratch, sizeof(scratch), "%sXXXXXX", TKT_ROOT);
    return krb5int_fcc_new_unique(context, scratch, id);
}

/* Return an alias to the pathname of the cache file. */
static const char * KRB5_CALLCONV
fcc_get_name(krb5_context context, krb5_ccache id)
{
    return ((fcc_data *)id->data)->filename;
}

/* Retrieve a copy of the default principal, if the cache is initialized. */
static krb5_error_code KRB5_CALLCONV
fcc_get_principal(krb5_context context, krb5_ccache id, krb5_principal *princ)
{
    krb5_error_code ret;
    fcc_data *data = id->data;
    FILE *fp = NULL;
    int version;

    k5_cc_mutex_lock(context, &data->lock);
    ret = open_cache_file(context, data->filename, FALSE, &fp);
    if (ret)
        goto cleanup;
    ret = read_header(context, fp, &version);
    if (ret)
        goto cleanup;
    ret = read_principal(context, fp, version, princ);

cleanup:
    (void)close_cache_file(context, fp);
    k5_cc_mutex_unlock(context, &data->lock);
    return set_errmsg_filename(context, ret, data->filename);
}

/* Search for a credential within the cache file. */
static krb5_error_code KRB5_CALLCONV
fcc_retrieve(krb5_context context, krb5_ccache id, krb5_flags whichfields,
             krb5_creds *mcreds, krb5_creds *creds)
{
    krb5_error_code ret;

    ret = k5_cc_retrieve_cred_default(context, id, whichfields, mcreds, creds);
    return set_errmsg_filename(context, ret, ((fcc_data *)id->data)->filename);
}

/* Store a credential in the cache file. */
static krb5_error_code KRB5_CALLCONV
fcc_store(krb5_context context, krb5_ccache id, krb5_creds *creds)
{
    krb5_error_code ret, ret2;
    fcc_data *data = id->data;
    FILE *fp = NULL;
    int version;
    struct k5buf buf = EMPTY_K5BUF;
    ssize_t nwritten;

    k5_cc_mutex_lock(context, &data->lock);

    /* Open the cache file for O_APPEND writing. */
    ret = open_cache_file(context, data->filename, TRUE, &fp);
    if (ret)
        goto cleanup;
    ret = read_header(context, fp, &version);
    if (ret)
        goto cleanup;

    /* Marshal the cred and write it to the file with a single append write. */
    k5_buf_init_dynamic(&buf);
    k5_marshal_cred(&buf, version, creds);
    ret = k5_buf_status(&buf);
    if (ret)
        goto cleanup;
    nwritten = write(fileno(fp), buf.data, buf.len);
    if (nwritten == -1)
        ret = interpret_errno(context, errno);
    if ((size_t)nwritten != buf.len)
        ret = KRB5_CC_IO;

    krb5_change_cache();

cleanup:
    k5_buf_free(&buf);
    ret2 = close_cache_file(context, fp);
    k5_cc_mutex_unlock(context, &data->lock);
    return set_errmsg_filename(context, ret ? ret : ret2, data->filename);
}

/* Non-functional stub for removing a cred from the cache file. */
static krb5_error_code KRB5_CALLCONV
fcc_remove_cred(krb5_context context, krb5_ccache cache, krb5_flags flags,
                krb5_creds *creds)
{
    return KRB5_CC_NOSUPP;
}

static krb5_error_code KRB5_CALLCONV
fcc_set_flags(krb5_context context, krb5_ccache id, krb5_flags flags)
{
    return 0;
}

static krb5_error_code KRB5_CALLCONV
fcc_get_flags(krb5_context context, krb5_ccache id, krb5_flags *flags)
{
    *flags = 0;
    return 0;
}

/* Prepare to iterate over the caches in the per-type collection. */
static krb5_error_code KRB5_CALLCONV
fcc_ptcursor_new(krb5_context context, krb5_cc_ptcursor *cursor)
{
    krb5_cc_ptcursor n = NULL;
    struct krb5_fcc_ptcursor_data *cdata = NULL;

    *cursor = NULL;

    n = malloc(sizeof(*n));
    if (n == NULL)
        return ENOMEM;
    n->ops = &krb5_fcc_ops;
    cdata = malloc(sizeof(*cdata));
    if (cdata == NULL) {
        free(n);
        return ENOMEM;
    }
    cdata->first = TRUE;
    n->data = cdata;
    *cursor = n;
    return 0;
}

/* Get the next cache in the per-type collection.  The FILE per-type collection
 * contains only the context's default cache if it is a file cache. */
static krb5_error_code KRB5_CALLCONV
fcc_ptcursor_next(krb5_context context, krb5_cc_ptcursor cursor,
                  krb5_ccache *cache_out)
{
    krb5_error_code ret;
    struct krb5_fcc_ptcursor_data *cdata = cursor->data;
    const char *defname, *residual;
    krb5_ccache cache;
    struct stat sb;

    *cache_out = NULL;
    if (!cdata->first)
        return 0;
    cdata->first = FALSE;

    defname = krb5_cc_default_name(context);
    if (!defname)
        return 0;

    /* Check if the default has type FILE or no type; find the residual. */
    if (strncmp(defname, "FILE:", 5) == 0)
        residual = defname + 5;
    else if (strchr(defname + 2, ':') == NULL)  /* Skip drive prefix if any. */
        residual = defname;
    else
        return 0;

    /* Don't yield a nonexistent default file cache. */
    if (stat(residual, &sb) != 0)
        return 0;

    ret = krb5_cc_resolve(context, defname, &cache);
    if (ret)
        return set_errmsg_filename(context, ret, defname);
    *cache_out = cache;
    return 0;
}

/* Release a per-type collection iteration cursor. */
static krb5_error_code KRB5_CALLCONV
fcc_ptcursor_free(krb5_context context, krb5_cc_ptcursor *cursor)
{
    if (*cursor == NULL)
        return 0;
    free((*cursor)->data);
    free(*cursor);
    *cursor = NULL;
    return 0;
}

/* Get the cache file's last modification time. */
static krb5_error_code KRB5_CALLCONV
fcc_last_change_time(krb5_context context, krb5_ccache id,
                     krb5_timestamp *change_time)
{
    krb5_error_code ret = 0;
    fcc_data *data = id->data;
    struct stat buf;

    *change_time = 0;

    k5_cc_mutex_lock(context, &data->lock);

    if (stat(data->filename, &buf) == -1)
        ret = interpret_errno(context, errno);
    else
        *change_time = (krb5_timestamp)buf.st_mtime;

    k5_cc_mutex_unlock(context, &data->lock);

    return set_errmsg_filename(context, ret, data->filename);
}

/* Lock the cache handle against other threads.  (This does not lock the cache
 * file against other processes.) */
static krb5_error_code KRB5_CALLCONV
fcc_lock(krb5_context context, krb5_ccache id)
{
    fcc_data *data = id->data;
    k5_cc_mutex_lock(context, &data->lock);
    return 0;
}

/* Unlock the cache handle. */
static krb5_error_code KRB5_CALLCONV
fcc_unlock(krb5_context context, krb5_ccache id)
{
    fcc_data *data = id->data;
    k5_cc_mutex_unlock(context, &data->lock);
    return 0;
}

/* Translate a system errno value to a Kerberos com_err code. */
static krb5_error_code
interpret_errno(krb5_context context, int errnum)
{
    krb5_error_code ret;

    switch (errnum) {
    case ENOENT:
    case ENOTDIR:
#ifdef ELOOP
    case ELOOP:
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
#endif
        ret = KRB5_FCC_NOFILE;
        break;
    case EPERM:
    case EACCES:
#ifdef EISDIR
    case EISDIR:                /* Mac doesn't have EISDIR */
#endif
    case EROFS:
        ret = KRB5_FCC_PERM;
        break;
    case EINVAL:
    case EEXIST:
    case EFAULT:
    case EBADF:
#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
#endif
        ret = KRB5_FCC_INTERNAL;
        break;
    /*
     * The rest all map to KRB5_CC_IO.  These errnos are listed to
     * document that they've been considered explicitly:
     *
     *  - EDQUOT
     *  - ENOSPC
     *  - EIO
     *  - ENFILE
     *  - EMFILE
     *  - ENXIO
     *  - EBUSY
     *  - ETXTBSY
     */
    default:
        ret = KRB5_CC_IO;
        break;
    }
    return ret;
}

const krb5_cc_ops krb5_fcc_ops = {
    0,
    "FILE",
    fcc_get_name,
    fcc_resolve,
    fcc_generate_new,
    fcc_initialize,
    fcc_destroy,
    fcc_close,
    fcc_store,
    fcc_retrieve,
    fcc_get_principal,
    fcc_start_seq_get,
    fcc_next_cred,
    fcc_end_seq_get,
    fcc_remove_cred,
    fcc_set_flags,
    fcc_get_flags,
    fcc_ptcursor_new,
    fcc_ptcursor_next,
    fcc_ptcursor_free,
    NULL, /* move */
    fcc_last_change_time,
    NULL, /* wasdefault */
    fcc_lock,
    fcc_unlock,
    NULL, /* switch_to */
};

#if defined(_WIN32)
/*
 * krb5_change_cache should be called after the cache changes.
 * A notification message is is posted out to all top level
 * windows so that they may recheck the cache based on the
 * changes made.  We register a unique message type with which
 * we'll communicate to all other processes.
 */

krb5_error_code
krb5_change_cache(void)
{
    PostMessage(HWND_BROADCAST, krb5_get_notification_message(), 0, 0);
    return 0;
}

unsigned int KRB5_CALLCONV
krb5_get_notification_message(void)
{
    static unsigned int message = 0;

    if (message == 0)
        message = RegisterWindowMessage(WM_KERBEROS5_CHANGED);

    return message;
}
#else /* _WIN32 */

krb5_error_code
krb5_change_cache(void)
{
    return 0;
}

unsigned int
krb5_get_notification_message(void)
{
    return 0;
}

#endif /* _WIN32 */

const krb5_cc_ops krb5_cc_file_ops = {
    0,
    "FILE",
    fcc_get_name,
    fcc_resolve,
    fcc_generate_new,
    fcc_initialize,
    fcc_destroy,
    fcc_close,
    fcc_store,
    fcc_retrieve,
    fcc_get_principal,
    fcc_start_seq_get,
    fcc_next_cred,
    fcc_end_seq_get,
    fcc_remove_cred,
    fcc_set_flags,
    fcc_get_flags,
    fcc_ptcursor_new,
    fcc_ptcursor_next,
    fcc_ptcursor_free,
    NULL, /* move */
    fcc_last_change_time,
    NULL, /* wasdefault */
    fcc_lock,
    fcc_unlock,
    NULL, /* switch_to */
};
