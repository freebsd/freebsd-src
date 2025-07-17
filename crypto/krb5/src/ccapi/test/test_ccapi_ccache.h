#ifndef _TEST_CCAPI_CCACHE_H_
#define _TEST_CCAPI_CCACHE_H_

#include "test_ccapi_globals.h"

int check_cc_ccache_release(void);
cc_int32 check_once_cc_ccache_release(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description);
int check_cc_ccache_destroy(void);
cc_int32 check_once_cc_ccache_destroy(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description);
int check_cc_ccache_set_default(void);
cc_int32 check_once_cc_ccache_set_default(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description);
int check_cc_ccache_get_credentials_version(void);
cc_int32 check_once_cc_ccache_get_credentials_version(cc_ccache_t ccache, cc_uint32 expected_cred_vers, cc_int32 expected_err, const char *description);
int check_cc_ccache_get_name(void);
cc_int32 check_once_cc_ccache_get_name(cc_ccache_t ccache, const char *expected_name, cc_int32 expected_err, const char *description);
int check_cc_ccache_get_principal(void);
cc_int32 check_once_cc_ccache_get_principal(cc_ccache_t ccache, cc_uint32 cred_vers, const char *expected_principal, cc_int32 expected_err, const char *description);
int check_cc_ccache_set_principal(void);
cc_int32 check_once_cc_ccache_set_principal(cc_ccache_t ccache, cc_uint32 cred_vers, const char *in_principal, cc_int32 expected_err, const char *description);

int check_cc_ccache_store_credentials(void);
cc_int32 check_once_cc_ccache_store_credentials(cc_ccache_t ccache, const cc_credentials_union *credentials, cc_int32 expected_err, const char *description);
int check_cc_ccache_remove_credentials(void);
cc_int32 check_once_cc_ccache_remove_credentials(cc_ccache_t ccache, cc_credentials_t in_creds, cc_int32 expected_err, const char *description);

int check_cc_ccache_new_credentials_iterator(void);
cc_int32 check_once_cc_ccache_new_credentials_iterator(cc_ccache_t ccache, cc_credentials_iterator_t *iterator, cc_int32 expected_err, const char *description);

int check_cc_ccache_get_change_time(void);
cc_int32 check_once_cc_ccache_get_change_time(cc_ccache_t ccache, cc_time_t *last_time, cc_int32 expected_err, const char *description);
int check_cc_ccache_get_last_default_time(void);
cc_int32 check_once_cc_ccache_get_last_default_time(cc_ccache_t ccache, cc_time_t *last_time, cc_int32 expected_err, const char *description);

int check_cc_ccache_move(void);
cc_int32 check_once_cc_ccache_move(cc_ccache_t source, cc_ccache_t destination, cc_int32 expected_err, const char *description);

int check_cc_ccache_compare(void);
cc_int32 check_once_cc_ccache_compare(cc_ccache_t ccache, cc_ccache_t compare_to, cc_uint32 *equal, cc_int32 expected_err, const char *description);

int check_cc_ccache_get_kdc_time_offset(void);
cc_int32 check_once_cc_ccache_get_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_time_t *time_offset, cc_int32 expected_err, const char *description);

int check_cc_ccache_set_kdc_time_offset(void);
cc_int32 check_once_cc_ccache_set_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_time_t time_offset, cc_int32 expected_err, const char *description);

int check_cc_ccache_clear_kdc_time_offset(void);
cc_int32 check_once_cc_ccache_clear_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_int32 expected_err, const char *description);

#endif /* _TEST_CCAPI_CCACHE_H_ */
