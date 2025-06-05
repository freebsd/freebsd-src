#ifndef _TEST_CCAPI_CONTEXT_H_
#define _TEST_CCAPI_CONTEXT_H_

#include "test_ccapi_globals.h"

int check_cc_initialize(void);
cc_int32 check_once_cc_initialize(cc_context_t *out_context, cc_int32 in_version, cc_int32 *out_supported_version, char const **out_vendor, cc_int32 expected_err, const char *description);
int check_cc_context_get_version(void);
cc_int32 check_once_cc_context_get_version(cc_context_t *out_context, cc_int32 in_version, cc_int32 *out_supported_version, char const **out_vendor, cc_int32 expected_err, const char *description);
int check_cc_context_release(void);
cc_int32 check_once_cc_context_release(cc_context_t *out_context, cc_int32 expected_err, const char *description);
int check_cc_context_get_change_time(void);
cc_int32 check_once_cc_context_get_change_time(cc_context_t context, cc_time_t *time, cc_int32 expected_err, const char *description);
int check_cc_context_get_default_ccache_name(void);
cc_int32 check_once_cc_context_get_default_ccache_name(cc_context_t context, cc_string_t *name, cc_int32 expected_err, const char *description);
int check_cc_context_open_ccache(void);
cc_int32 check_once_cc_context_open_ccache(cc_context_t context, const char *name, cc_ccache_t *ccache, cc_int32 expected_err, const char *description);
int check_cc_context_open_default_ccache(void);
cc_int32 check_once_cc_context_open_default_ccache(cc_context_t context, cc_ccache_t *ccache, cc_int32 expected_err, const char *description);
int check_cc_context_create_ccache(void);
cc_int32 check_once_cc_context_create_ccache(cc_context_t context, const char *name, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description);
int check_cc_context_create_default_ccache(void);
cc_int32 check_once_cc_context_create_default_ccache(cc_context_t context, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description);
int check_cc_context_create_new_ccache(void);
cc_int32 check_once_cc_context_create_new_ccache(cc_context_t context, cc_int32 should_be_default, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description);
int check_cc_context_new_ccache_iterator(void);
cc_int32 check_once_cc_context_new_ccache_iterator(cc_context_t context, cc_ccache_iterator_t *iterator, cc_int32 expected_err, const char *description);

int check_cc_context_compare(void);
cc_int32 check_once_cc_context_compare(cc_context_t context, cc_context_t compare_to, cc_uint32 *equal, cc_int32 expected_err, const char *description);

#endif /* _TEST_CCAPI_CONTEXT_H_ */
