/* $FreeBSD$ */

#include "stand.h"

#ifdef BSD4_4
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

/* shortened version of warn */
static const char *program_name;

void 
set_program_name(n)
	const char *n;
{
	if ((program_name = strrchr(n, '/')) != NULL)
		program_name++;
	else
		program_name = n;
}

void 
warn(const char *fmt, ...)
{
	va_list ap;
	int interrno;

	va_start(ap, fmt);

	interrno = errno;
	(void)fprintf(stderr, "%s: ", program_name);
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(interrno));

	va_end(ap);
}

void 
warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)fprintf(stderr, "%s: ", program_name);
	if (fmt != NULL) 
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);
}

#endif
