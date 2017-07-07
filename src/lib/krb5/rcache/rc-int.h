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

#ifndef __KRB5_RCACHE_INT_H__
#define __KRB5_RCACHE_INT_H__

int krb5int_rc_finish_init(void);

void krb5int_rc_terminate(void);

struct krb5_rc_st {
    krb5_magic magic;
    const struct _krb5_rc_ops *ops;
    krb5_pointer data;
    k5_mutex_t lock;
};

struct _krb5_rc_ops {
    krb5_magic magic;
    char *type;
    krb5_error_code (KRB5_CALLCONV *init)(
        krb5_context,
        krb5_rcache,
        krb5_deltat); /* create */
    krb5_error_code (KRB5_CALLCONV *recover)(
        krb5_context,
        krb5_rcache); /* open */
    krb5_error_code (KRB5_CALLCONV *recover_or_init)(
        krb5_context,
        krb5_rcache,
        krb5_deltat);
    krb5_error_code (KRB5_CALLCONV *destroy)(
        krb5_context,
        krb5_rcache);
    krb5_error_code (KRB5_CALLCONV *close)(
        krb5_context,
        krb5_rcache);
    krb5_error_code (KRB5_CALLCONV *store)(
        krb5_context,
        krb5_rcache,
        krb5_donot_replay *);
    krb5_error_code (KRB5_CALLCONV *expunge)(
        krb5_context,
        krb5_rcache);
    krb5_error_code (KRB5_CALLCONV *get_span)(
        krb5_context,
        krb5_rcache,
        krb5_deltat *);
    char *(KRB5_CALLCONV *get_name)(
        krb5_context,
        krb5_rcache);
    krb5_error_code (KRB5_CALLCONV *resolve)(
        krb5_context,
        krb5_rcache,
        char *);
};

typedef struct _krb5_rc_ops krb5_rc_ops;

krb5_error_code krb5_rc_register_type(krb5_context, const krb5_rc_ops *);

extern const krb5_rc_ops krb5_rc_dfl_ops;
extern const krb5_rc_ops krb5_rc_none_ops;

#endif /* __KRB5_RCACHE_INT_H__ */
