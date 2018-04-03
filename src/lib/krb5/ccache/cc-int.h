/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc-int.h */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * file-based credential cache routines. */

#ifndef __KRB5_CCACHE_H__
#define __KRB5_CCACHE_H__

#include "k5-int.h"

struct _krb5_ccache {
    krb5_magic magic;
    const struct _krb5_cc_ops *ops;
    krb5_pointer data;
};

krb5_error_code
k5_cc_retrieve_cred_default(krb5_context, krb5_ccache, krb5_flags,
                            krb5_creds *, krb5_creds *);

krb5_boolean
krb5int_cc_creds_match_request(krb5_context, krb5_flags whichfields, krb5_creds *mcreds, krb5_creds *creds);

int
krb5int_cc_initialize(void);

void
krb5int_cc_finalize(void);

/*
 * Cursor for iterating over ccache types
 */
struct krb5_cc_typecursor;
typedef struct krb5_cc_typecursor *krb5_cc_typecursor;

krb5_error_code
krb5int_cc_typecursor_new(krb5_context context, krb5_cc_typecursor *cursor);

krb5_error_code
krb5int_cc_typecursor_next(
    krb5_context context,
    krb5_cc_typecursor cursor,
    const struct _krb5_cc_ops **ops);

krb5_error_code
krb5int_cc_typecursor_free(
    krb5_context context,
    krb5_cc_typecursor *cursor);

/* reentrant mutex used by krb5_cc_* functions */
typedef struct _k5_cc_mutex {
    k5_mutex_t lock;
    krb5_context owner;
    krb5_int32 refcount;
} k5_cc_mutex;

#define K5_CC_MUTEX_PARTIAL_INITIALIZER         \
    { K5_MUTEX_PARTIAL_INITIALIZER, NULL, 0 }

krb5_error_code
k5_cc_mutex_init(k5_cc_mutex *m);

krb5_error_code
k5_cc_mutex_finish_init(k5_cc_mutex *m);

#define k5_cc_mutex_destroy(M)                  \
    k5_mutex_destroy(&(M)->lock);

void
k5_cc_mutex_assert_locked(krb5_context context, k5_cc_mutex *m);

void
k5_cc_mutex_assert_unlocked(krb5_context context, k5_cc_mutex *m);

void
k5_cc_mutex_lock(krb5_context context, k5_cc_mutex *m);

void
k5_cc_mutex_unlock(krb5_context context, k5_cc_mutex *m);

extern k5_cc_mutex krb5int_mcc_mutex;
extern k5_cc_mutex krb5int_krcc_mutex;
extern k5_cc_mutex krb5int_cc_file_mutex;

#ifdef USE_CCAPI_V3
extern krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_lock
(krb5_context context);

extern krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_unlock
(krb5_context context);
#endif

void
k5_cc_mutex_force_unlock(k5_cc_mutex *m);

void
k5_cccol_force_unlock(void);

krb5_error_code
krb5int_fcc_new_unique(krb5_context context, char *template, krb5_ccache *id);

krb5_error_code
ccselect_hostname_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable);

krb5_error_code
ccselect_realm_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);

krb5_error_code
ccselect_k5identity_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable);

krb5_error_code
k5_unmarshal_cred(const unsigned char *data, size_t len, int version,
                  krb5_creds *creds);

krb5_error_code
k5_unmarshal_princ(const unsigned char *data, size_t len, int version,
                   krb5_principal *princ_out);

void
k5_marshal_cred(struct k5buf *buf, int version, krb5_creds *creds);

void
k5_marshal_mcred(struct k5buf *buf, krb5_creds *mcred);

void
k5_marshal_princ(struct k5buf *buf, int version, krb5_principal princ);

/*
 * Per-type ccache cursor.
 */
struct krb5_cc_ptcursor_s {
    const struct _krb5_cc_ops *ops;
    krb5_pointer data;
};
typedef struct krb5_cc_ptcursor_s *krb5_cc_ptcursor;

struct _krb5_cc_ops {
    krb5_magic magic;
    char *prefix;
    const char * (KRB5_CALLCONV *get_name)(krb5_context, krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *resolve)(krb5_context, krb5_ccache *,
                                             const char *);
    krb5_error_code (KRB5_CALLCONV *gen_new)(krb5_context, krb5_ccache *);
    krb5_error_code (KRB5_CALLCONV *init)(krb5_context, krb5_ccache,
                                          krb5_principal);
    krb5_error_code (KRB5_CALLCONV *destroy)(krb5_context, krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *close)(krb5_context, krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *store)(krb5_context, krb5_ccache,
                                           krb5_creds *);
    krb5_error_code (KRB5_CALLCONV *retrieve)(krb5_context, krb5_ccache,
                                              krb5_flags, krb5_creds *,
                                              krb5_creds *);
    krb5_error_code (KRB5_CALLCONV *get_princ)(krb5_context, krb5_ccache,
                                               krb5_principal *);
    krb5_error_code (KRB5_CALLCONV *get_first)(krb5_context, krb5_ccache,
                                               krb5_cc_cursor *);
    krb5_error_code (KRB5_CALLCONV *get_next)(krb5_context, krb5_ccache,
                                              krb5_cc_cursor *, krb5_creds *);
    krb5_error_code (KRB5_CALLCONV *end_get)(krb5_context, krb5_ccache,
                                             krb5_cc_cursor *);
    krb5_error_code (KRB5_CALLCONV *remove_cred)(krb5_context, krb5_ccache,
                                                 krb5_flags, krb5_creds *);
    krb5_error_code (KRB5_CALLCONV *set_flags)(krb5_context, krb5_ccache,
                                               krb5_flags);
    krb5_error_code (KRB5_CALLCONV *get_flags)(krb5_context, krb5_ccache,
                                               krb5_flags *);
    krb5_error_code (KRB5_CALLCONV *ptcursor_new)(krb5_context,
                                                  krb5_cc_ptcursor *);
    krb5_error_code (KRB5_CALLCONV *ptcursor_next)(krb5_context,
                                                   krb5_cc_ptcursor,
                                                   krb5_ccache *);
    krb5_error_code (KRB5_CALLCONV *ptcursor_free)(krb5_context,
                                                   krb5_cc_ptcursor *);
    krb5_error_code (KRB5_CALLCONV *move)(krb5_context, krb5_ccache,
                                          krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *lastchange)(krb5_context,
                                                krb5_ccache, krb5_timestamp *);
    krb5_error_code (KRB5_CALLCONV *wasdefault)(krb5_context, krb5_ccache,
                                                krb5_timestamp *);
    krb5_error_code (KRB5_CALLCONV *lock)(krb5_context, krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *unlock)(krb5_context, krb5_ccache);
    krb5_error_code (KRB5_CALLCONV *switch_to)(krb5_context, krb5_ccache);
};

extern const krb5_cc_ops *krb5_cc_dfl_ops;

#endif /* __KRB5_CCACHE_H__ */
