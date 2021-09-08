/* public domain */

#include "includes.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "log.h"

void
sshfatal(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, ...)
{
	va_list ap;

	if (showfunc)
		fprintf(stderr, "%s: ", func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (suffix != NULL)
		fprintf(stderr, ": %s", suffix);
	fputc('\n', stderr);
	_exit(1);
}
