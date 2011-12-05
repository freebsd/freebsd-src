/* $FreeBSD$ */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>

#include "dhcpd.h"

extern jmp_buf env;

void
error(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	longjmp(env, 1);
}

int
warning(char *fmt, ...)
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
note(char *fmt, ...)
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
