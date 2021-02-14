/* public domain */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

void fatal(char *fmt, ...);

void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	_exit(1);
}
