/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/ipc_stream.c */
/*
 * Copyright 2006, 2007, 2009 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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

#ifdef _WIN32
#include <winsock2.h>
#endif
#include "k5-ipc_stream.h"

#if !defined(htonll)
#define htonll(x) k5_htonll(x)
#endif

#if !defined(ntohll)
#define ntohll(x) k5_ntohll(x)
#endif

/* Add debugging later */
#define k5_check_error(x) (x)

struct k5_ipc_stream_s {
    char *data;
    uint64_t size;
    uint64_t max_size;
};

static const struct k5_ipc_stream_s k5_ipc_stream_initializer = { NULL, 0, 0 };

#define K5_IPC_STREAM_SIZE_INCREMENT 128

/* ------------------------------------------------------------------------ */

static uint32_t krb5int_ipc_stream_reallocate (k5_ipc_stream io_stream,
                                               uint64_t      in_new_size)
{
    int32_t err = 0;
    uint64_t new_max_size = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        uint64_t old_max_size = io_stream->max_size;
        new_max_size = io_stream->max_size;

        if (in_new_size > old_max_size) {
            /* Expand the stream */
            while (in_new_size > new_max_size) {
                new_max_size += K5_IPC_STREAM_SIZE_INCREMENT;
            }


        } else if ((in_new_size + K5_IPC_STREAM_SIZE_INCREMENT) < old_max_size) {
            /* Shrink the array, but never drop below K5_IPC_STREAM_SIZE_INCREMENT */
            while ((in_new_size + K5_IPC_STREAM_SIZE_INCREMENT) < new_max_size &&
                   (new_max_size > K5_IPC_STREAM_SIZE_INCREMENT)) {
                new_max_size -= K5_IPC_STREAM_SIZE_INCREMENT;
            }
        }
    }

    if (!err && new_max_size != io_stream->max_size) {
        char *data = io_stream->data;

        if (!data) {
            data = malloc (new_max_size * sizeof (*data));
        } else {
            data = realloc (data, new_max_size * sizeof (*data));
        }

        if (data) {
            io_stream->data = data;
            io_stream->max_size = new_max_size;
        } else {
            err = k5_check_error (ENOMEM);
        }
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

int32_t krb5int_ipc_stream_new (k5_ipc_stream *out_stream)
{
    int32_t err = 0;
    k5_ipc_stream stream = NULL;

    if (!out_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        stream = malloc (sizeof (*stream));
        if (stream) {
            *stream = k5_ipc_stream_initializer;
        } else {
            err = k5_check_error (ENOMEM);
        }
    }

    if (!err) {
        *out_stream = stream;
        stream = NULL;
    }

    krb5int_ipc_stream_release (stream);

    return k5_check_error (err);
}


/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_release (k5_ipc_stream io_stream)
{
    int32_t err = 0;

    if (!err && io_stream) {
        free (io_stream->data);
        free (io_stream);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

uint64_t krb5int_ipc_stream_size (k5_ipc_stream in_stream)
{
    return in_stream ? in_stream->size : 0;
}


/* ------------------------------------------------------------------------ */

const char *krb5int_ipc_stream_data (k5_ipc_stream in_stream)
{
    return in_stream ? in_stream->data : NULL;
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read (k5_ipc_stream  io_stream,
                                  void         *io_data,
                                  uint64_t     in_size)
{
    int32_t err = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!io_data  ) { err = k5_check_error (EINVAL); }

    if (!err) {
        if (in_size > io_stream->size) {
            err = k5_check_error (EINVAL);
        }
    }

    if (!err) {
        memcpy (io_data, io_stream->data, in_size);
        memmove (io_stream->data, &io_stream->data[in_size],
                 io_stream->size - in_size);

        err = krb5int_ipc_stream_reallocate (io_stream, io_stream->size - in_size);

        if (!err) {
            io_stream->size -= in_size;
        }
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write (k5_ipc_stream  io_stream,
                                   const void   *in_data,
                                   uint64_t     in_size)
{
    int32_t err = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!in_data  ) { err = k5_check_error (EINVAL); }

    if (!err) {
        /* Security check: Do not let the caller overflow the length */
        if (in_size > (UINT64_MAX - io_stream->size)) {
            err = k5_check_error (EINVAL);
        }
    }

    if (!err) {
        err = krb5int_ipc_stream_reallocate (io_stream, io_stream->size + in_size);
    }

    if (!err) {
        memcpy (&io_stream->data[io_stream->size], in_data, in_size);
        io_stream->size += in_size;
    }

    return k5_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

void krb5int_ipc_stream_free_string (char *in_string)
{
    free (in_string);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_string (k5_ipc_stream   io_stream,
                                         char         **out_string)
{
    int32_t err = 0;
    uint32_t length = 0;
    char *string = NULL;

    if (!io_stream ) { err = k5_check_error (EINVAL); }
    if (!out_string) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &length);
    }

    if (!err) {
        string = malloc (length);
        if (!string) { err = k5_check_error (ENOMEM); }
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, string, length);
    }

    if (!err) {
        *out_string = string;
        string = NULL;
    }

    free (string);

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_string (k5_ipc_stream  io_stream,
                                          const char    *in_string)
{
    int32_t err = 0;
    uint32_t length = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!in_string) { err = k5_check_error (EINVAL); }

    if (!err) {
        length = strlen (in_string) + 1;

        err = krb5int_ipc_stream_write_uint32 (io_stream, length);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_string, length);
    }

    return k5_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_int32 (k5_ipc_stream  io_stream,
                                        int32_t       *out_int32)
{
    int32_t err = 0;
    int32_t int32 = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!out_int32) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, &int32, sizeof (int32));
    }

    if (!err) {
        *out_int32 = ntohl (int32);
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_int32 (k5_ipc_stream io_stream,
                                         int32_t       in_int32)
{
    int32_t err = 0;
    int32_t int32 = htonl (in_int32);

    if (!io_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, &int32, sizeof (int32));
    }

    return k5_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_uint32 (k5_ipc_stream  io_stream,
                                         uint32_t      *out_uint32)
{
    int32_t err = 0;
    uint32_t uint32 = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!out_uint32) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, &uint32, sizeof (uint32));
    }

    if (!err) {
        *out_uint32 = ntohl (uint32);
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_uint32 (k5_ipc_stream io_stream,
                                          uint32_t      in_uint32)
{
    int32_t err = 0;
    int32_t uint32 = htonl (in_uint32);

    if (!io_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, &uint32, sizeof (uint32));
    }

    return k5_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_int64 (k5_ipc_stream  io_stream,
                                        int64_t       *out_int64)
{
    int32_t err = 0;
    uint64_t int64 = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!out_int64) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, &int64, sizeof (int64));
    }

    if (!err) {
        *out_int64 = ntohll (int64);
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_int64 (k5_ipc_stream io_stream,
                                         int64_t     in_int64)
{
    int32_t err = 0;
    int64_t int64 = htonll (in_int64);

    if (!io_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, &int64, sizeof (int64));
    }

    return k5_check_error (err);
}


#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_read_uint64 (k5_ipc_stream  io_stream,
                                         uint64_t     *out_uint64)
{
    int32_t err = 0;
    uint64_t uint64 = 0;

    if (!io_stream) { err = k5_check_error (EINVAL); }
    if (!out_uint64) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, &uint64, sizeof (uint64));
    }

    if (!err) {
        *out_uint64 = ntohll (uint64);
    }

    return k5_check_error (err);
}

/* ------------------------------------------------------------------------ */

uint32_t krb5int_ipc_stream_write_uint64 (k5_ipc_stream io_stream,
                                          uint64_t      in_uint64)
{
    int32_t err = 0;
    int64_t uint64 = htonll (in_uint64);

    if (!io_stream) { err = k5_check_error (EINVAL); }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, &uint64, sizeof (uint64));
    }

    return k5_check_error (err);
}
