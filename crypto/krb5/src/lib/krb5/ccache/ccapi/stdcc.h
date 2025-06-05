/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef __KRB5_STDCC_H__
#define __KRB5_STDCC_H__

#if defined(_WIN32) || defined(USE_CCAPI)

#include "k5-int.h"     /* loads krb5.h */
#include "../cc-int.h"

#include <CredentialsCache.h>

#define kStringLiteralLen 255

/* globals to be exported */
extern krb5_cc_ops krb5_cc_stdcc_ops;

/*
 * structure to stash in the cache's data field
 */
typedef struct _stdccCacheData {
    char *cache_name;
    cc_ccache_t NamedCache;
} stdccCacheData, *stdccCacheDataPtr;


/* function prototypes  */

void krb5_stdcc_shutdown(void);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_close
(krb5_context, krb5_ccache id );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_destroy
(krb5_context, krb5_ccache id );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_end_seq_get
(krb5_context, krb5_ccache id , krb5_cc_cursor *cursor );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_generate_new
(krb5_context, krb5_ccache *id );

const char * KRB5_CALLCONV krb5_stdccv3_get_name
(krb5_context, krb5_ccache id );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_get_principal
(krb5_context, krb5_ccache id , krb5_principal *princ );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_initialize
(krb5_context, krb5_ccache id , krb5_principal princ );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_next_cred
(krb5_context,
 krb5_ccache id ,
 krb5_cc_cursor *cursor ,
 krb5_creds *creds );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_resolve
(krb5_context, krb5_ccache *id , const char *residual );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_retrieve
(krb5_context,
 krb5_ccache id ,
 krb5_flags whichfields ,
 krb5_creds *mcreds ,
 krb5_creds *creds );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_start_seq_get
(krb5_context, krb5_ccache id , krb5_cc_cursor *cursor );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_store
(krb5_context, krb5_ccache id , krb5_creds *creds );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_set_flags
(krb5_context, krb5_ccache id , krb5_flags flags );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_get_flags
(krb5_context, krb5_ccache id , krb5_flags *flags );

krb5_error_code KRB5_CALLCONV krb5_stdccv3_remove
(krb5_context, krb5_ccache id , krb5_flags flags, krb5_creds *creds);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_ptcursor_new
(krb5_context context, krb5_cc_ptcursor *cursor);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_ptcursor_next
(krb5_context context, krb5_cc_ptcursor cursor, krb5_ccache *ccache);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_ptcursor_free
(krb5_context context, krb5_cc_ptcursor *cursor);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_lock
(krb5_context, krb5_ccache id);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_unlock
(krb5_context, krb5_ccache id);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_lock
(krb5_context context);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_context_unlock
(krb5_context context);

krb5_error_code KRB5_CALLCONV krb5_stdccv3_switch_to
(krb5_context context, krb5_ccache id);

#endif /* defined(_WIN32) || defined(USE_CCAPI) */

#endif
