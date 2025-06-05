/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc-int.h */
/*
 * Copyright 2004 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

/* This file contains constant and function declarations used in the
 * file-based replay cache routines. */

#ifndef RC_INT_H
#define RC_INT_H

typedef struct {
    const char *type;
    krb5_error_code (*resolve)(krb5_context context, const char *residual,
                               void **rcdata_out);
    void (*close)(krb5_context context, void *rcdata);
    krb5_error_code (*store)(krb5_context, void *rcdata, const krb5_data *tag);
} krb5_rc_ops;

struct krb5_rc_st {
    krb5_magic magic;
    const krb5_rc_ops *ops;
    char *name;
    void *data;
};

extern const krb5_rc_ops k5_rc_dfl_ops;
extern const krb5_rc_ops k5_rc_file2_ops;
extern const krb5_rc_ops k5_rc_none_ops;

/* Check and store a replay record in an open (but not locked) file descriptor,
 * using the file2 format.  fd is assumed to be at offset 0. */
krb5_error_code k5_rcfile2_store(krb5_context context, int fd,
                                 const krb5_data *tag_data);

#endif /* RC_INT_H */
