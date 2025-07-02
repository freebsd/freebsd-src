/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/CredentialsCache2.h */
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

/*
 * This is backwards compatibility for CCache API v2 clients to be able to run
 * against the CCache API v3 library
 */

#ifndef CCAPI_V2_H
#define CCAPI_V2_H

#include <CredentialsCache.h>

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#include <TargetConditionals.h>
#include <AvailabilityMacros.h>
#ifdef DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#define CCAPI_DEPRECATED DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER
#endif
#endif

#ifndef CCAPI_DEPRECATED
#define CCAPI_DEPRECATED
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__) || defined(__i386__) || defined(__x86_64__))
#pragma pack(push,2)
#endif

/* Some old types get directly mapped to new types */

typedef cc_context_d apiCB;
typedef cc_ccache_d ccache_p;
typedef cc_credentials_iterator_d ccache_cit_creds;
typedef cc_ccache_iterator_d ccache_cit_ccache;
typedef cc_data cc_data_compat;
typedef cc_int32 cc_cred_vers;
typedef cc_int32 cc_result;

/* This doesn't exist in API v3 */
typedef cc_uint32 cc_flags;

/* Credentials types are visible to the caller so we have to keep binary compatibility */

typedef struct cc_credentials_v5_compat {
    char*                       client;
    char*                       server;
    cc_data_compat              keyblock;
    cc_time_t                   authtime;
    cc_time_t                   starttime;
    cc_time_t                   endtime;
    cc_time_t                   renew_till;
    cc_uint32                   is_skey;
    cc_uint32                   ticket_flags;
    cc_data_compat**            addresses;
    cc_data_compat              ticket;
    cc_data_compat              second_ticket;
    cc_data_compat**            authdata;
} cc_credentials_v5_compat;

enum {
    KRB_NAME_SZ = 40,
    KRB_INSTANCE_SZ = 40,
    KRB_REALM_SZ = 40
};

typedef union cred_ptr_union_compat {
    cc_credentials_v5_compat* pV5Cred;
} cred_ptr_union_compat;

typedef struct cred_union {
    cc_int32              cred_type;  /* cc_cred_vers */
    cred_ptr_union_compat cred;
} cred_union;

/* NC info structure is gone in v3 */

struct infoNC {
    char*       name;
    char*       principal;
    cc_int32    vers;
};

typedef struct infoNC infoNC;

/* Some old type names */

typedef cc_credentials_v5_compat cc_creds;
struct ccache_cit;
typedef struct ccache_cit ccache_cit;

enum {
    CC_API_VER_2 = ccapi_version_2
};

enum {
    CC_NOERROR,
    CC_BADNAME,
    CC_NOTFOUND,
    CC_END,
    CC_IO,
    CC_WRITE,
    CC_NOMEM,
    CC_FORMAT,
    CC_LOCKED,
    CC_BAD_API_VERSION,
    CC_NO_EXIST,
    CC_NOT_SUPP,
    CC_BAD_PARM,
    CC_ERR_CACHE_ATTACH,
    CC_ERR_CACHE_RELEASE,
    CC_ERR_CACHE_FULL,
    CC_ERR_CRED_VERSION
};

enum {
    CC_CRED_UNKNOWN,
    /* CC_CRED_V4, */
    CC_CRED_V5,
    CC_CRED_MAX
};

enum {
    CC_LOCK_UNLOCK = 1,
    CC_LOCK_READER = 2,
    CC_LOCK_WRITER = 3,
    CC_LOCK_NOBLOCK = 16
};

CCACHE_API cc_int32
cc_shutdown (apiCB **io_context)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_get_NC_info (apiCB    *in_context,
                infoNC ***out_info)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_get_change_time (apiCB     *in_context,
                    cc_time_t *out_change_time)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_open (apiCB       *in_context,
         const char  *in_name,
         cc_int32     in_version,
         cc_uint32    in_flags,
         ccache_p   **out_ccache)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_create (apiCB       *in_context,
           const char  *in_name,
           const char  *in_principal,
           cc_int32     in_version,
           cc_uint32    in_flags,
           ccache_p   **out_ccache)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_close (apiCB     *in_context,
          ccache_p **ioCCache)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_destroy (apiCB     *in_context,
            ccache_p **io_ccache)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_NCs_begin (apiCB       *in_context,
                        ccache_cit **out_nc_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_NCs_next (apiCB       *in_context,
                       ccache_p   **out_ccache,
                       ccache_cit  *in_nc_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_NCs_end (apiCB       *in_context,
                      ccache_cit **io_nc_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_get_name (apiCB     *in_context,
             ccache_p  *in_ccache,
             char     **out_name)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_get_cred_version (apiCB    *in_context,
                     ccache_p *in_ccache,
                     cc_int32 *out_version)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_set_principal (apiCB    *in_context,
                  ccache_p *in_ccache,
                  cc_int32  in_version,
                  char     *in_principal)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_get_principal (apiCB     *in_context,
                  ccache_p  *in_ccache,
                  char     **out_principal)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_store (apiCB      *in_context,
          ccache_p   *in_ccache,
          cred_union  in_credentials)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_remove_cred (apiCB      *in_context,
                ccache_p   *in_ccache,
                cred_union  in_credentials)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_creds_begin (apiCB           *in_context,
                          const ccache_p  *in_ccache,
                          ccache_cit     **out_ccache_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_creds_next (apiCB       *in_context,
                         cred_union **out_cred_union,
                         ccache_cit  *in_ccache_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_seq_fetch_creds_end (apiCB       *in_context,
                        ccache_cit **io_ccache_iterator)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_free_principal (apiCB  *in_context,
                   char  **io_principal)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_free_name (apiCB  *in_context,
              char  **io_name)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_free_creds (apiCB       *in_context,
               cred_union **io_cred_union)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_free_NC_info (apiCB    *in_context,
                 infoNC ***io_info)
    CCAPI_DEPRECATED;

CCACHE_API cc_int32
cc_lock_request (apiCB          *in_context,
                 const ccache_p *in_ccache,
                 const cc_int32  in_lock_type)
    CCAPI_DEPRECATED;

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__) || defined(__i386__) || defined(__x86_64__))
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CCAPI_V2_H */
