
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>

#include "dhcpd.h"

extern jmp_buf env;
int warnings_occurred;

void
error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	longjmp(env, 1);
}

int
warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	/*
	 * The original warning() would return "ret" here. We do this to
	 * check warnings explicitly.
	 */
	longjmp(env, 1);
}

int
note(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	return ret;
}

int
parse_warn(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	return ret;
}

void
bootp(struct packet *packet)
{
}

void
dhcp(struct packet *packet)
{
}
