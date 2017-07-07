/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * winccld.h -- the dynamic loaded version of the ccache DLL
 */


#ifndef KRB5_WINCCLD_H_
#define KRB5_WINCCLD_H_

#ifdef USE_CCAPI_V3
#include <CredentialsCache.h>
#else

#ifndef CC_API_VER2
#define CC_API_VER2
#endif

#include "cacheapi.h"
#endif

#ifdef USE_CCAPI_V3
typedef CCACHE_API cc_int32 (*FP_cc_initialize) (
    cc_context_t*           outContext,
    cc_int32                inVersion,
    cc_int32*               outSupportedVersion,
    char const**            outVendor);
#else
typedef cc_int32 (*FP_cc_initialize)(apiCB**, const cc_int32,
                                     cc_int32*, const char**);
typedef cc_int32 (*FP_cc_shutdown)(apiCB**);
typedef cc_int32 (*FP_cc_get_change_time)(apiCB*, cc_time_t*);
typedef cc_int32 (*FP_cc_create)(apiCB*, const char*, const char*,
                                 const enum cc_cred_vers, const cc_int32, ccache_p**);
typedef cc_int32 (*FP_cc_open)(apiCB*, const char*, const enum cc_cred_vers,
                               const cc_int32, ccache_p**);
typedef cc_int32 (*FP_cc_close)(apiCB*, ccache_p**);
typedef cc_int32 (*FP_cc_destroy)(apiCB*, ccache_p**);
typedef cc_int32 (*FP_cc_seq_fetch_NCs)(apiCB*, ccache_p**, ccache_cit**);
typedef cc_int32 (*FP_cc_seq_fetch_NCs_begin)(apiCB*, ccache_cit**);
typedef cc_int32 (*FP_cc_seq_fetch_NCs_next)(apiCB*, ccache_p**, ccache_cit*);
typedef cc_int32 (*FP_cc_seq_fetch_NCs_end)(apiCB*, ccache_cit**);
typedef cc_int32 (*FP_cc_get_NC_info)(apiCB*, struct _infoNC***);
typedef cc_int32 (*FP_cc_free_NC_info)(apiCB*, struct _infoNC***);
typedef cc_int32 (*FP_cc_get_name)(apiCB*, const ccache_p*, char**);
typedef cc_int32 (*FP_cc_set_principal)(apiCB*, const ccache_p*,
                                        const enum cc_cred_vers, const char*);
typedef cc_int32 (*FP_cc_get_principal)(apiCB*, ccache_p*, char**);
typedef cc_int32 (*FP_cc_get_cred_version)(apiCB*, const ccache_p*,
                                           enum cc_cred_vers*);
typedef cc_int32 (*FP_cc_lock_request)(apiCB*, const ccache_p*,
                                       const cc_int32);
typedef cc_int32 (*FP_cc_store)(apiCB*, const ccache_p*, const cred_union);
typedef cc_int32 (*FP_cc_remove_cred)(apiCB*, const ccache_p*,
                                      const cred_union);
typedef cc_int32 (*FP_cc_seq_fetch_creds)(apiCB*, const ccache_p*,
                                          cred_union**, ccache_cit**);
typedef cc_int32 (*FP_cc_seq_fetch_creds_begin)(apiCB*, const ccache_p*,
                                                ccache_cit**);
typedef cc_int32 (*FP_cc_seq_fetch_creds_next)(apiCB*, cred_union**,
                                               ccache_cit*);
typedef cc_int32 (*FP_cc_seq_fetch_creds_end)(apiCB*, ccache_cit**);
typedef cc_int32 (*FP_cc_free_principal)(apiCB*, char**);
typedef cc_int32 (*FP_cc_free_name)(apiCB*, char** name);
typedef cc_int32 (*FP_cc_free_creds)(apiCB*, cred_union** pCred);
#endif

#ifdef KRB5_WINCCLD_C_
typedef struct _FUNC_INFO {
    void** func_ptr_var;
    char* func_name;
} FUNC_INFO;

#define DECL_FUNC_PTR(x) FP_##x p##x
#define MAKE_FUNC_INFO(x) { (void**) &p##x, #x }
#define END_FUNC_INFO { 0, 0 }
#else
#define DECL_FUNC_PTR(x) extern FP_##x p##x
#endif

DECL_FUNC_PTR(cc_initialize);
#ifndef USE_CCAPI_V3
DECL_FUNC_PTR(cc_shutdown);
DECL_FUNC_PTR(cc_get_change_time);
DECL_FUNC_PTR(cc_create);
DECL_FUNC_PTR(cc_open);
DECL_FUNC_PTR(cc_close);
DECL_FUNC_PTR(cc_destroy);
#if 0 /* Not used */
#ifdef CC_API_VER2
DECL_FUNC_PTR(cc_seq_fetch_NCs_begin);
DECL_FUNC_PTR(cc_seq_fetch_NCs_next);
DECL_FUNC_PTR(cc_seq_fetch_NCs_end);
#else
DECL_FUNC_PTR(cc_seq_fetch_NCs);
#endif
DECL_FUNC_PTR(cc_get_NC_info);
DECL_FUNC_PTR(cc_free_NC_info);
#endif
DECL_FUNC_PTR(cc_get_name);
DECL_FUNC_PTR(cc_set_principal);
DECL_FUNC_PTR(cc_get_principal);
DECL_FUNC_PTR(cc_get_cred_version);
#if 0 /* Not used */
DECL_FUNC_PTR(cc_lock_request);
#endif
DECL_FUNC_PTR(cc_store);
DECL_FUNC_PTR(cc_remove_cred);
#ifdef CC_API_VER2
DECL_FUNC_PTR(cc_seq_fetch_creds_begin);
DECL_FUNC_PTR(cc_seq_fetch_creds_next);
DECL_FUNC_PTR(cc_seq_fetch_creds_end);
#else
DECL_FUNC_PTR(cc_seq_fetch_creds);
#endif
DECL_FUNC_PTR(cc_free_principal);
DECL_FUNC_PTR(cc_free_name);
DECL_FUNC_PTR(cc_free_creds);
#endif

#ifdef KRB5_WINCCLD_C_
FUNC_INFO krbcc_fi[] = {
    MAKE_FUNC_INFO(cc_initialize),
#ifndef USE_CCAPI_V3
    MAKE_FUNC_INFO(cc_shutdown),
    MAKE_FUNC_INFO(cc_get_change_time),
    MAKE_FUNC_INFO(cc_create),
    MAKE_FUNC_INFO(cc_open),
    MAKE_FUNC_INFO(cc_close),
    MAKE_FUNC_INFO(cc_destroy),
#if 0 /* Not used */
    MAKE_FUNC_INFO(cc_seq_fetch_NCs),
    MAKE_FUNC_INFO(cc_get_NC_info),
    MAKE_FUNC_INFO(cc_free_NC_info),
#endif
    MAKE_FUNC_INFO(cc_get_name),
    MAKE_FUNC_INFO(cc_set_principal),
    MAKE_FUNC_INFO(cc_get_principal),
    MAKE_FUNC_INFO(cc_get_cred_version),
#if 0 /* Not used */
    MAKE_FUNC_INFO(cc_lock_request),
#endif
    MAKE_FUNC_INFO(cc_store),
    MAKE_FUNC_INFO(cc_remove_cred),
#ifdef CC_API_VER2
    MAKE_FUNC_INFO(cc_seq_fetch_creds_begin),
    MAKE_FUNC_INFO(cc_seq_fetch_creds_next),
    MAKE_FUNC_INFO(cc_seq_fetch_creds_end),
#else
    MAKE_FUNC_INFO(cc_seq_fetch_creds),
#endif
    MAKE_FUNC_INFO(cc_free_principal),
    MAKE_FUNC_INFO(cc_free_name),
    MAKE_FUNC_INFO(cc_free_creds),
#endif
    END_FUNC_INFO
};
#undef MAKE_FUNC_INFO
#undef END_FUNC_INFO
#else

#define cc_initialize pcc_initialize
#ifndef USE_CCAPI_V3
#define cc_shutdown pcc_shutdown
#define cc_get_change_time pcc_get_change_time
#define cc_create pcc_create
#define cc_open pcc_open
#define cc_close pcc_close
#define cc_destroy pcc_destroy
#if 0 /* Not used */
#ifdef CC_API_VER2
#define cc_seq_fetch_NCs_begin pcc_seq_fetch_NCs_begin
#define cc_seq_fetch_NCs_next pcc_seq_fetch_NCs_next
#define cc_seq_fetch_NCs_end pcc_seq_fetch_NCs_end
#else
#define cc_seq_fetch_NCs pcc_seq_fetch_NCs
#endif
#define cc_get_NC_info pcc_get_NC_info
#define cc_free_NC_info pcc_free_NC_info
#endif /* End of Not used */
#define cc_get_name pcc_get_name
#define cc_set_principal pcc_set_principal
#define cc_get_principal pcc_get_principal
#define cc_get_cred_version pcc_get_cred_version
#if 0 /* Not used */
#define cc_lock_request pcc_lock_request
#endif
#define cc_store pcc_store
#define cc_remove_cred pcc_remove_cred
#ifdef CC_API_VER2
#define cc_seq_fetch_creds_begin pcc_seq_fetch_creds_begin
#define cc_seq_fetch_creds_next pcc_seq_fetch_creds_next
#define cc_seq_fetch_creds_end pcc_seq_fetch_creds_end
#else
#define cc_seq_fetch_creds pcc_seq_fetch_creds
#endif
#define cc_free_principal pcc_free_principal
#define cc_free_name pcc_free_name
#define cc_free_creds pcc_free_creds
#endif
#endif

#undef DECL_FUNC_PTR

#endif /* KRB5_WINCCLD_H_ */
