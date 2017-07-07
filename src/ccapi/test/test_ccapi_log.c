#ifndef _TEST_CCAPI_LOG_C_
#define _TEST_CCAPI_LOG_C_

#include "test_ccapi_log.h"

void _log_error_v(const char *file, int line, const char *format, va_list ap)
{
	fprintf(stdout, "\n\t%s:%d: ", file, line);
	if (!format) {
		fprintf(stdout, "An unknown error occurred");
	} else {
		vfprintf(stdout, format, ap);
	}
	fflush(stdout);
}

void _log_error(const char *file, int line, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	_log_error_v(file, line, format, ap);
	va_end(ap);
}

void test_header(const char *msg) {
	if (msg != NULL) {
		fprintf(stdout, "\nChecking %s... ", msg);
		fflush(stdout);
	}
}

void test_footer(const char *msg, int err) {
	if (msg != NULL) {
		if (!err) {
			fprintf(stdout, "OK\n");
		}
		else {
			fprintf(stdout, "\n*** %d failure%s in %s ***\n", err, (err == 1) ? "" : "s", msg);
		}
	}
}



#endif /* _TEST_CCAPI_LOG_C_ */
