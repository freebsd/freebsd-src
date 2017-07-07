/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-ipc_stream.h */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
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

#ifndef K5_IPC_STREAM_H
#define K5_IPC_STREAM_H

#include "k5-platform.h"

struct k5_ipc_stream_s;
typedef struct k5_ipc_stream_s *k5_ipc_stream;


int32_t krb5int_ipc_stream_new (k5_ipc_stream *out_stream);

uint32_t krb5int_ipc_stream_release (k5_ipc_stream io_stream);

uint64_t krb5int_ipc_stream_size (k5_ipc_stream in_stream);

const char *krb5int_ipc_stream_data (k5_ipc_stream in_stream);

uint32_t krb5int_ipc_stream_read (k5_ipc_stream  in_stream,
                                  void          *io_data,
                                  uint64_t       in_size);
uint32_t krb5int_ipc_stream_write (k5_ipc_stream  in_stream,
                                   const void    *in_data,
                                   uint64_t       in_size);

uint32_t krb5int_ipc_stream_read_string (k5_ipc_stream   io_stream,
                                         char          **out_string);
uint32_t krb5int_ipc_stream_write_string (k5_ipc_stream  io_stream,
                                          const char    *in_string);
void krb5int_ipc_stream_free_string (char *in_string);

uint32_t krb5int_ipc_stream_read_int32 (k5_ipc_stream  io_stream,
                                        int32_t       *out_int32);
uint32_t krb5int_ipc_stream_write_int32 (k5_ipc_stream io_stream,
                                         int32_t       in_int32);

uint32_t krb5int_ipc_stream_read_uint32 (k5_ipc_stream  io_stream,
                                         uint32_t      *out_uint32);
uint32_t krb5int_ipc_stream_write_uint32 (k5_ipc_stream io_stream,
                                          uint32_t      in_uint32);

uint32_t krb5int_ipc_stream_read_int64 (k5_ipc_stream  io_stream,
                                        int64_t       *out_int64);
uint32_t krb5int_ipc_stream_write_int64 (k5_ipc_stream io_stream,
                                         int64_t       in_int64);

uint32_t krb5int_ipc_stream_read_uint64 (k5_ipc_stream  io_stream,
                                         uint64_t      *out_uint64);
uint32_t krb5int_ipc_stream_write_uint64 (k5_ipc_stream io_stream,
                                          uint64_t      in_uint64);

#endif /* K5_IPC_STREAM_H */
