#include "test_ccapi_check.h"

int _check_if(int expression, const char *file, int line, const char *expression_string, const char *format, ...) {
	if (expression) {
		failure_count++;
		// call with NULL format to get a generic error message
		if (format == NULL) {
			_log_error(file, line, expression_string);
		}
		// call with format and varargs for a more useful error message
		else {
			va_list ap;
			va_start(ap, format);
			_log_error_v(file, line, format, ap);
			va_end(ap);
		}

		if (current_test_activity) {
			fprintf(stdout, " (%s)", current_test_activity);
		}
	}

	return (expression != 0);
}

int array_contains_int(cc_int32 *array, int size, cc_int32 value) {
	if (array != NULL && size > 0) {
		int i = 0;
		while (i < size && array[i] != value) {
			i++;
		}
		if (i < size) {
			return 1;
		}
	}
	return 0;
}
