#ifndef _TEST_CCAPI_ITERATORS_H_
#define _TEST_CCAPI_ITERATORS_H_

#include "test_ccapi_globals.h"

int check_cc_ccache_iterator_next(void);
cc_int32 check_once_cc_ccache_iterator_next(cc_ccache_iterator_t iterator, cc_uint32 expected_count, cc_int32 expected_err, const char *description);

int check_cc_credentials_iterator_next(void);
cc_int32 check_once_cc_credentials_iterator_next(cc_credentials_iterator_t iterator, cc_uint32 expected_count, cc_int32 expected_err, const char *description);

#endif /* _TEST_CCAPI_ITERATORS_H_ */
