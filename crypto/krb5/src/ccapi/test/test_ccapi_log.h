#ifndef _TEST_CCAPI_LOG_H_
#define _TEST_CCAPI_LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include "test_ccapi_globals.h"

#define log_error(format, ...) \
		_log_error(__FILE__, __LINE__, format , ## __VA_ARGS__)

void _log_error_v(const char *file, int line, const char *format, va_list ap);
void _log_error(const char *file, int line, const char *format, ...)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
__attribute__ ((__format__ (__printf__, 3, 4)))
#endif
;
void test_header(const char *msg);
void test_footer(const char *msg, int err);

#endif /* _TEST_CCAPI_LOG_H_ */
