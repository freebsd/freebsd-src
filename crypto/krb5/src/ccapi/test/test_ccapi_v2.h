#ifndef _TEST_CCAPI_V2_H_
#define _TEST_CCAPI_V2_H_

#include "test_ccapi_globals.h"
#ifdef TARGET_OS_MAC
#include <Kerberos/CredentialsCache2.h>
#else
#include <CredentialsCache2.h>
#endif


int check_cc_shutdown(void);
cc_result check_once_cc_shutdown(apiCB **out_context, cc_result expected_err, const char *description);

int check_cc_get_change_time(void);
cc_int32 check_once_cc_get_change_time(apiCB *context, cc_time_t *time, cc_result expected_err, const char *description);

int check_cc_open(void);
cc_result check_once_cc_open(apiCB *context, const char *name, cc_int32 version, ccache_p **ccache, cc_result expected_err, const char *description);

int check_cc_create(void);
cc_result check_once_cc_create(apiCB  *context, const char *name, cc_int32 cred_vers, const char *principal, ccache_p **ccache, cc_int32 expected_err, const char *description);

int check_cc_close(void);
cc_result check_once_cc_close(apiCB *context, ccache_p *ccache, cc_result expected_err, const char *description);

int check_cc_destroy(void);
cc_result check_once_cc_destroy(apiCB *context, ccache_p *ccache, cc_int32 expected_err, const char *description);

int check_cc_get_cred_version(void);
cc_result check_once_cc_get_cred_version(apiCB *context, ccache_p *ccache, cc_int32 expected_cred_vers, cc_int32 expected_err, const char *description);

int check_cc_get_name(void);
cc_int32 check_once_cc_get_name(apiCB *context, ccache_p *ccache, const char *expected_name, cc_int32 expected_err, const char *description);

int check_cc_get_principal(void);
cc_result check_once_cc_get_principal(apiCB *context,
                                      ccache_p *ccache,
                                      const char *expected_principal,
                                      cc_int32 expected_err,
                                      const char *description);

int check_cc_set_principal(void);
cc_int32 check_once_cc_set_principal(apiCB *context, ccache_p *ccache, cc_int32 cred_vers, const char *in_principal, cc_int32 expected_err, const char *description);

int check_cc_store(void);
cc_result check_once_cc_store(apiCB *context, ccache_p *ccache, const cred_union in_creds, cc_int32 expected_err, const char *description);

int check_cc_remove_cred(void);
cc_result check_once_cc_remove_cred(apiCB *context, ccache_p *ccache, cred_union in_creds, cc_int32 expected_err, const char *description);

int check_cc_seq_fetch_NCs_begin(void);
cc_result check_once_cc_seq_fetch_NCs_begin(apiCB *context, ccache_cit **iterator, cc_result expected_err, const char *description);

int check_cc_seq_fetch_NCs_next(void);
cc_result check_once_cc_seq_fetch_NCs_next(apiCB *context, ccache_cit *iterator, cc_uint32 expected_count, cc_result expected_err, const char *description);

int check_cc_get_NC_info(void);
cc_result check_once_cc_get_NC_info(apiCB *context,
                                    const char *expected_name,
                                    const char *expected_principal,
                                    cc_int32 expected_version,
                                    cc_uint32 expected_count,
                                    cc_result expected_err,
                                    const char *description);

int check_cc_seq_fetch_creds_begin(void);
cc_result check_once_cc_seq_fetch_creds_begin(apiCB *context, ccache_p *ccache, ccache_cit **iterator, cc_result expected_err, const char *description);

int check_cc_seq_fetch_creds_next(void);
cc_result check_once_cc_seq_fetch_creds_next(apiCB *context, ccache_cit *iterator, cc_uint32 expected_count, cc_result expected_err, const char *description);

#endif /* _TEST_CCAPI_V2_H_ */
