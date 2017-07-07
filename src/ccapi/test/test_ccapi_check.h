#ifndef _TEST_CCAPI_CHECK_H_
#define _TEST_CCAPI_CHECK_H_

#include <stdio.h>
#include <stdarg.h>
#include "test_ccapi_log.h"
#include "test_ccapi_globals.h"

int _check_if(int expression, const char *file, int line, const char *expression_string, const char *format, ...);

#define check_int(a, b) \
		check_if(a != b, NULL)

/*
 *	if expression evaluates to true, check_if increments the failure_count and prints:
 *
 *	check_if(a!=a, NULL);
 *	==> "/path/to/file:line: a!=a"
 *
 *	check_if(a!=a, "This shouldn't be happening");
 *	==> "/path/to/file:line: This shouldn't be happening"
 *
 *	check_if(a!=a, "This has happened %d times now", 3);
 *	==> "/path/to/file:line: This has happened 3 times now"
*/

#define check_if(expression, format, ...) \
		_check_if(expression, __FILE__, __LINE__, #expression, format , ## __VA_ARGS__)

#define check_if_not(expression, format, ...) \
		check_if(!(expression), format, ## __VA_ARGS__)

// first check if err is what we were expecting to get back
// then check if err is even in the set of errors documented for the function
#define check_err(err, expected_err, possible_return_values) \
		do { \
			check_if(err != expected_err, "unexpected error %s (%d), expected %s (%d)", translate_ccapi_error(err), err, translate_ccapi_error(expected_err), expected_err); \
			check_if_not(array_contains_int(possible_return_values, possible_ret_val_count, err), "error not documented as a possible return value: %s (%d)", translate_ccapi_error(err), err); \
		} while( 0 )

int array_contains_int(cc_int32 *array, int size, cc_int32 value);

#endif /* _TEST_CCAPI_CHECK_H_ */
