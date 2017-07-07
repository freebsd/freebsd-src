#ifndef _TEST_CCAPI_UTIL_H_
#define _TEST_CCAPI_UTIL_H_

#include "test_ccapi_globals.h"
#include "test_ccapi_log.h"

cc_int32 destroy_all_ccaches(cc_context_t context);

cc_int32 new_v5_creds_union(cc_credentials_union *out_union, const char *realm);
void release_v5_creds_union(cc_credentials_union *creds_union);
int compare_v5_creds_unions(const cc_credentials_union *a, const cc_credentials_union *b);

#endif /* _TEST_CCAPI_UTIL_H_ */
